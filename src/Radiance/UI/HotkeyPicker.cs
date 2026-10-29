using System.Runtime.InteropServices;

namespace Radiance.UI;

/// <summary>
/// Hotkey capture field. Click to start listening, press a combination,
/// Esc to cancel. Reports whether the combination could actually be registered,
/// so a conflict with another app is caught before the user relies on it.
/// </summary>
public sealed class HotkeyPicker : ThemedControl
{
    private const int WM_HOTKEY = 0x0312;
    private static int _probeId = 0x7000;

    [DllImport("user32.dll")] private static extern bool RegisterHotKey(IntPtr h, int id, uint mods, uint vk);
    [DllImport("user32.dll")] private static extern bool UnregisterHotKey(IntPtr h, int id);

    private sealed class ProbeWindow : NativeWindow
    {
        public ProbeWindow() => CreateHandle(new CreateParams { Caption = "RadianceHotkeyProbe" });
    }

    private static readonly ProbeWindow Probe = new();

    private bool _capturing;
    private HotkeyConfig _config = new();

    public event EventHandler? BindingChanged;
    public event EventHandler<string>? Rejected;

    public HotkeyConfig Config
    {
        get => _config;
        set { _config = value; Invalidate(); }
    }

    public HotkeyPicker()
    {
        Height = 44;
        Cursor = Cursors.Hand;
        TabStop = true;
        AccessibleRole = AccessibleRole.PushButton;
        AccessibleName = "Hotkey";
    }

    protected override void OnClick(EventArgs e)
    {
        _capturing = !_capturing;
        if (_capturing) Focus();
        Invalidate();
        base.OnClick(e);
    }

    protected override void OnLostFocus(EventArgs e)
    {
        _capturing = false;
        Invalidate();
        base.OnLostFocus(e);
    }

    protected override bool IsInputKey(Keys keyData) => _capturing || base.IsInputKey(keyData);

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (!_capturing) { base.OnKeyDown(e); return; }

        e.Handled = true;
        e.SuppressKeyPress = true;

        if (e.KeyCode == Keys.Escape)
        {
            _capturing = false;
            Invalidate();
            return;
        }

        var key = e.KeyCode;
        if (key is Keys.ControlKey or Keys.Menu or Keys.ShiftKey or Keys.LWin or Keys.RWin)
            return; // wait for the real key

        if (key == Keys.None) return;

        var cfg = new HotkeyConfig
        {
            Enabled = true,
            Key = key,
            Ctrl = e.Control,
            Alt = e.Alt,
            Shift = e.Shift
        };

        string? error = Validate(cfg);
        if (error is not null)
        {
            Rejected?.Invoke(this, error);
            return;
        }

        _config = cfg;
        _capturing = false;
        Invalidate();
        BindingChanged?.Invoke(this, EventArgs.Empty);
    }

    /// <summary>Returns null when the combination is usable, else a reason.</summary>
    public static string? Validate(HotkeyConfig cfg)
    {
        if (!cfg.Enabled || cfg.Key == Keys.None) return null;

        if (!cfg.Ctrl && !cfg.Alt && !cfg.Shift)
            return "Use at least one of Ctrl, Alt or Shift.";

        if (cfg.Key is Keys.LWin or Keys.RWin)
            return "The Windows key cannot be used.";

        // probe the real registration: if it fails, something else owns it
        int id = ++_probeId;
        uint mods = (cfg.Ctrl ? 0x2u : 0u) | (cfg.Alt ? 0x1u : 0u) | (cfg.Shift ? 0x4u : 0u);

        if (!RegisterHotKey(Probe.Handle, id, mods, (uint)cfg.Key))
            return $"{cfg.Display} is already used by another program.";

        UnregisterHotKey(Probe.Handle, id);
        return null;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

        var r = new Rectangle(0, 4, Width - 1, Height - 9);
        Draw.FillRounded(g, r, 8, _capturing ? P.SurfaceAlt : P.Surface);
        Draw.StrokeRounded(g, r, 8, _capturing ? P.Accent : P.Border, _capturing ? 2f : 1f);

        string text = _capturing
            ? "Press a combination…  (Esc to cancel)"
            : _config.Display;

        Draw.Text(g, text, Theme.Body, _capturing ? P.Accent : P.Text,
            new Rectangle(12, r.Y, r.Width - 24, r.Height));
    }
}