using Radiance.Core;

namespace Radiance.UI;

/// <summary>
/// Small always-on-top window with the vibrance slider, so it can be changed
/// without opening the main window. Esc or losing focus closes it.
/// </summary>
public sealed class QuickPopup : Form
{
    private readonly ShellForm _shell;
    private readonly SliderRow _vibrance = new() { Label = "Vibrance", Min = 0, Max = 100 };
    private readonly SliderRow _saturation = new() { Label = "Saturation", Min = 0, Max = 300 };
    private readonly Caption _hint = new() { Text = "Esc to close" };

    public QuickPopup(ShellForm shell)
    {
        _shell = shell;

        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        Size = new Size(360, 190);
        BackColor = Theme.Palette.Surface;
        DoubleBuffered = true;
        Padding = new Padding(18, 14, 18, 12);

        _vibrance.Value = shell.Settings.Vibrance;
        _saturation.Value = shell.Settings.Saturation;

        _vibrance.ValueChanged += (_, _) => Apply();
        _saturation.ValueChanged += (_, _) => Apply();

        _vibrance.Dock = DockStyle.Top;
        _saturation.Dock = DockStyle.Top;
        _hint.Dock = DockStyle.Bottom;

        Controls.Add(_saturation);
        Controls.Add(_vibrance);
        Controls.Add(_hint);

        KeyPreview = true;
        Deactivate += (_, _) => Close();
    }

    protected override bool ShowWithoutActivation => false;

    protected override void OnPaint(PaintEventArgs e)
    {
        using var pen = new Pen(Theme.Palette.Border);
        e.Graphics.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Escape) { Close(); return; }
        base.OnKeyDown(e);
    }

    /// <summary>Shows near the cursor, clamped to the working area.</summary>
    public void ShowNearCursor()
    {
        var screen = Screen.FromPoint(Cursor.Position).WorkingArea;
        int x = Math.Clamp(Cursor.Position.X - Width / 2, screen.Left + 8, screen.Right - Width - 8);
        int y = Math.Clamp(Cursor.Position.Y - Height / 2, screen.Top + 8, screen.Bottom - Height - 8);
        Location = new Point(x, y);
        Show();
        Activate();
    }

    private void Apply()
    {
        var s = _shell.Settings;
        s.Vibrance = _vibrance.Value;
        s.Saturation = _saturation.Value;
        _shell.Pipeline.Apply(s);
    }
}