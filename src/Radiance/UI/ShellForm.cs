using System.Drawing.Drawing2D;
using Radiance.Core;
using Radiance.UI.Pages;

namespace Radiance.UI;

/// <summary>Common base for the content pages.</summary>
public abstract class Page : UserControl
{
    protected Page()
    {
        Dock = DockStyle.Fill;
        BackColor = Theme.Palette.Background;
        DoubleBuffered = true;
        Theme.Changed += OnThemeChanged;
    }

    public abstract string Title { get; }
    public abstract string Subtitle { get; }

    /// <summary>Called when the page becomes visible, so it can refresh live state.</summary>
    public virtual void OnShown() { }

    private void OnThemeChanged()
    {
        // Pages paint their own background: transparent pages showed the
        // container colour, which never changed with the theme.
        BackColor = Theme.Palette.Background;
        Invalidate(true);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) Theme.Changed -= OnThemeChanged;
        base.Dispose(disposing);
    }
}

public sealed class ShellForm : Form
{
    private readonly AppSettings _settings;
    private readonly ColorPipeline _pipeline = new();
    private readonly HotkeyManager _hotkeys = new();
    private readonly HotkeyManager _crossHotkeys = new();
    private readonly HotkeyManager _quickHotkeys = new();
    private readonly System.Windows.Forms.Timer _audioTimer = new() { Interval = 1000 };

    private bool _forceClose;
    private readonly object _discordLock = new();

    public PhoneRemote Phone { get; } = new();
    public DiscordPresence Discord { get; private set; } = new(Branding.DiscordAppId);

    public void ForceClose()
    {
        _forceClose = true;
        Close();
    }

    public void SaveSettings()
    {
        try
        {
            if (_settings.AutoSaveSettings)
            {
                if (_current is not null)
                    _settings.LastPage = _current.Title;

                if (WindowState == FormWindowState.Normal)
                {
                    _settings.WindowX = Location.X;
                    _settings.WindowY = Location.Y;
                    _settings.WindowWidth = Width;
                    _settings.WindowHeight = Height;
                    _settings.WindowMaximised = false;
                }
                else if (WindowState == FormWindowState.Maximized)
                {
                    _settings.WindowMaximised = true;
                }

                _settings.Save();
            }
            UpdatePresence();
        }
        catch { }
    }

    public void StartPhone()
    {
        Phone.Start(v =>
        {
            _settings.Vibrance = v;
            _pipeline.Apply(_settings);
            NotifySettingsChanged();
        }, () => _settings.Vibrance, theme =>
        {
            Theme.Set(theme == "light" ? ThemeMode.Light : ThemeMode.Dark);
            _settings.Theme = Theme.Mode;
            _settings.Save();
            NotifySettingsChanged();
        });
    }

    public void StopPhone() => Phone.Stop();

    /// <summary>Emergency off switch for the per-pixel overlay.</summary>
    public void DisableOverlay()
    {
        _settings.UseShaderOverlay = false;
        _settings.Save();
        _pipeline.Apply(_settings);

        NotifySettingsChanged();
    }

    /// <summary>Recreates the Discord client (the application id can change).</summary>
    /// <summary>Connects / reconnects rich presence using the built-in application id.</summary>
    public void ApplyDiscord()
    {
        lock (_discordLock)
        {
            Discord.Dispose();
            Discord = new DiscordPresence(Branding.DiscordAppId);

            if (Discord.Connect())
            {
                UpdatePresence();
            }
        }
    }

    private string? _lastLookName;
    private float? _preLimitVolume;

    public void UpdatePresence()
    {
        string details = _lastLookName is { Length: > 0 }
            ? $"Preset: {_lastLookName}"
            : $"Vibrance: {_settings.Vibrance}%";

        if (_settings.Crosshair.Enabled)
            details += " | Crosshair ON";

        lock (_discordLock)
        {
            if (!Discord.Connected && !Discord.Connect()) return;
            Discord.SetActivity(Environment.ProcessId, details);
        }
    }

    /// <summary>Publishes the applied look, retrying if Discord starts later.</summary>
    private void PublishPresence(string? name)
    {
        _lastLookName = name;
        UpdatePresence();
    }
    private readonly TrayIcon _tray;

    private readonly Panel _sidebar = new();
    private readonly Panel _header = new();
    private readonly Panel _host = new();
    private readonly Panel _body = new();
    private readonly Panel _main = new();
    private readonly List<NavButton> _navs = new();
    private readonly Dictionary<Page, NavButton> _navOf = new();

    private FlatButton? _themeButton;

    /// <summary>
    /// Raised when something outside a page changed the settings, so every copy
    /// of a control (theme, vibrance, crosshair) can refresh itself.
    /// </summary>
    public event Action? SettingsChanged;

    public void NotifySettingsChanged()
    {
        SaveSettings();
        SettingsChanged?.Invoke();
    }

    private Page? _current;
    private Page? _home;
    private int _cycle;

    public ColorPipeline Pipeline => _pipeline;
    public AppSettings Settings => _settings;
    public IReadOnlyList<Page> Pages => _navOf.Keys.ToList();

    private bool _allowVisible = true;

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hWnd);

    public void ShowMainWindow()
    {
        if (InvokeRequired)
        {
            try { BeginInvoke(new Action(ShowMainWindow)); } catch { }
            return;
        }

        _allowVisible = true;
        if (!Visible)
        {
            Show();
        }

        if (_settings.WindowMaximised)
            WindowState = FormWindowState.Maximized;
        else if (WindowState == FormWindowState.Minimized)
            WindowState = FormWindowState.Normal;

        TopMost = true;
        TopMost = false;
        BringToFront();
        Activate();
        SetForegroundWindow(Handle);
    }

    protected override void SetVisibleCore(bool value)
    {
        if (!_allowVisible)
        {
            value = false;
            if (!IsHandleCreated) CreateHandle();
        }
        base.SetVisibleCore(value);
    }

    public ShellForm(bool startInTray = false)
    {
        _settings = AppSettings.Load();
        if (startInTray || _settings.StartMinimised)
            _allowVisible = false;

        // If the last run died while the per-pixel overlay was live, the screen may
        // have been left frozen or colour-shifted. Start without it.
        if (ShaderOverlay.RecoveredFromUncleanRun())
        {
            _settings.UseShaderOverlay = false;
            _settings.Save();
        }

        Theme.Set(_settings.Theme);

        Text = Branding.Name;
        Icon = BrandAssets.AppIcon;
        MinimumSize = new Size(940, 640);
        if (_settings.WindowWidth >= 940 && _settings.WindowHeight >= 640)
            Size = new Size(_settings.WindowWidth, _settings.WindowHeight);
        else
            Size = new Size(1100, 740);

        if (_settings.WindowX >= 0 && _settings.WindowY >= 0 &&
            Screen.AllScreens.Any(s => s.WorkingArea.Contains(new Point(_settings.WindowX + 50, _settings.WindowY + 50))))
        {
            StartPosition = FormStartPosition.Manual;
            Location = new Point(_settings.WindowX, _settings.WindowY);
        }
        else
        {
            StartPosition = FormStartPosition.CenterScreen;
        }

        if (_settings.WindowMaximised)
            WindowState = FormWindowState.Maximized;

        FormBorderStyle = FormBorderStyle.None;
        DoubleBuffered = true;
        BackColor = Theme.Palette.Background;

        _pipeline.Initialize();
        _pipeline.Apply(_settings);

        StartupManager.Sync(_settings.StartWithWindows);

        Build();
        AddPages();

        _tray = new TrayIcon(this);
        _hotkeys.Pressed = CycleProfile;
        _crossHotkeys.Pressed = ToggleCrosshair;
        _quickHotkeys.Pressed = ShowQuickPopup;
        ApplyHotkey();
        ApplyCrosshairHotkey();
        ApplyQuickHotkey();

        ApplyStoredImageTheme();

        _audioTimer.Tick += (_, _) =>
        {
            if (_settings.AudioLimiterEnabled)
            {
                // remember where it was, so switching off can put it back
                _preLimitVolume ??= AudioLimiter.GetVolume();
                AudioLimiter.Clamp(_settings.AudioLimitPercent / 100f);
            }
            else if (_preLimitVolume is float restore)
            {
                AudioLimiter.SetVolume(restore);
                _preLimitVolume = null;
            }

            // Some display changes clear the gamma ramp; put the look back.
            _pipeline.Reassert();

            // Discord may start after us; keep trying quietly or reassert presence.
            lock (_discordLock)
            {
                if (!Discord.Connected)
                {
                    if (Discord.Connect())
                        UpdatePresence();
                }
            }
        };
        _audioTimer.Start();

        if (_settings.PhoneRemoteEnabled) StartPhone();
        Task.Run(ApplyDiscord);

        Theme.Changed += Restyle;
        ShowPage(_home!);
        _ = Task.Run(CheckUpdatesQuietly);

        if (!_settings.OnboardingDone)
            Shown += (_, _) => RunOnboarding();

    }

    // ---- wheel ----

    /// <summary>
    /// Windows sends the wheel to the focused window, which is often not the one
    /// the pointer is over. Route it using the wheel message's own coordinates,
    /// so it works no matter where the real cursor is.
    /// </summary>
    private bool ScrollAt(Point clientPoint, int delta)
    {
        var control = DeepestAt(this, clientPoint);

        for (var t = control; t is not null; t = t.Parent)
        {
            if (t is DesignGallery) return false;   // the gallery scrolls itself
            if (t is CardStack stack)
            {
                stack.ScrollBy(delta);
                return true;
            }
        }

        return false;
    }

    /// <summary>Deepest visible control under a point, in this form's client space.</summary>
    private static Control? DeepestAt(Control root, Point point)
    {
        Control? found = null;

        foreach (Control child in root.Controls)
        {
            if (!child.Visible) continue;

            var bounds = child.Bounds;
            if (!bounds.Contains(point)) continue;

            var inside = new Point(point.X - bounds.X, point.Y - bounds.Y);
            found = DeepestAt(child, inside) ?? child;
        }

        return found;
    }

    protected override void OnMouseWheel(MouseEventArgs e)
    {
        if (ScrollAt(new Point(e.X, e.Y), e.Delta)) return;
        base.OnMouseWheel(e);
    }

    // ---- layout ----

    private void Build()
    {
        var titleBar = new TitleBar(this) { Dock = DockStyle.Top, Height = 48 };

        _body.Dock = DockStyle.Fill;
        _body.BackColor = Theme.Palette.Background;

        _sidebar.Dock = DockStyle.Left;
        _sidebar.Width = 216;
        _sidebar.Paint += PaintSidebar;

        _main.Dock = DockStyle.Fill;
        _main.BackColor = Theme.Palette.Background;

        _header.Dock = DockStyle.Top;
        _header.Height = 92;
        _header.Paint += PaintHeader;

        _host.Dock = DockStyle.Fill;
        _host.Padding = new Padding(24, 0, 18, 18);
        _host.BackColor = Theme.Palette.Background;

        _main.Controls.Add(_host);
        _main.Controls.Add(_header);

        _body.Controls.Add(_main);
        _body.Controls.Add(_sidebar);

        Controls.Add(_body);
        Controls.Add(titleBar);
    }

    private void AddPages()
    {
        var pages = new Page[]
        {
            new VibrancePage(this),
            new CrosshairPage(this),
            new MonitorPage(this),
            new HardwarePage(this),
            new ProfilesPage(this),
            new LibraryPage(this),
            new CustomizePage(this),
            new SettingsPage(this),
        };

        _home = pages[0];

        if (!string.IsNullOrWhiteSpace(_settings.LastPage))
            _home = pages.FirstOrDefault(p => p.Title.Equals(_settings.LastPage, StringComparison.OrdinalIgnoreCase)) ?? _home;

        // debug aid: RADIANCE_START=Crosshair opens straight on that page
        var wanted = Environment.GetEnvironmentVariable("RADIANCE_START");
        if (!string.IsNullOrWhiteSpace(wanted))
            _home = pages.FirstOrDefault(p => p.Title.Equals(wanted, StringComparison.OrdinalIgnoreCase)) ?? _home;

        // build nav in reverse so Dock.Top keeps visual order
        for (int i = pages.Length - 1; i >= 0; i--)
        {
            var page = pages[i];
            var nav = new NavButton
            {
                Text = page.Title,
                Dock = DockStyle.Top,
                Glyph = Glyphs.For(page.Title)
            };
            nav.Click += (_, _) => ShowPage(page);
            _sidebar.Controls.Add(nav);
            _navs.Insert(0, nav);
            _navOf[page] = nav;
        }

        // consistent top padding above the first nav item
        _sidebar.Controls.Add(new Panel { Dock = DockStyle.Top, Height = 14, BackColor = Color.Transparent });

        BuildSidebarFooter();
    }

    private void BuildSidebarFooter()
    {
        var footer = new Panel { Dock = DockStyle.Bottom, Height = 64, BackColor = Color.Transparent };

        var theme = new FlatButton { Text = "Light theme", Accent = false, Width = 176, Height = 30, Left = 20, Top = 8 };
        _themeButton = theme;

        theme.Click += (_, _) =>
        {
            var mode = Theme.Mode == ThemeMode.Dark ? ThemeMode.Light : ThemeMode.Dark;
            _settings.Theme = mode;
            Theme.Set(mode);              // every listener follows, including Settings
            NotifySettingsChanged();
            _settings.Save();
        };

        theme.Text = Theme.Mode == ThemeMode.Dark ? "Light theme" : "Dark theme";

        var version = new Caption
        {
            Text = $"v{Branding.Version}",
            Left = 22,
            Top = 42,
            Width = 176,
            Height = 18
        };

        footer.Controls.Add(theme);
        footer.Controls.Add(version);
        _sidebar.Controls.Add(footer);
    }

    private void PaintSidebar(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        var p = Theme.Palette;
        g.Clear(p.Sidebar);
        using var pen = new Pen(p.Border);
        g.DrawLine(pen, _sidebar.Width - 1, 0, _sidebar.Width - 1, _sidebar.Height);
    }

    private void PaintHeader(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Theme.Palette.Background);

        if (_current is null) return;

        g.SmoothingMode = SmoothingMode.AntiAlias;
        Draw.Text(g, _current.Title, Theme.Title, Theme.Palette.Text,
            new Rectangle(24, 16, _header.Width - 48, 38));
        Draw.Text(g, _current.Subtitle, Theme.Body, Theme.Palette.TextMuted,
            new Rectangle(24, 54, _header.Width - 48, 22));
    }

    public void ShowPage(Page page)
    {
        if (_current == page) return;

        if (_current is not null)
        {
            _host.Controls.Remove(_current);
            _current.Visible = false;
        }

        _current = page;
        _host.Controls.Add(page);
        page.Visible = true;
        page.BringToFront();

        foreach (var kv in _navOf) kv.Value.Selected = kv.Key == page;

        _header.Invalidate();
        page.OnShown();
        SaveSettings();
    }

    private void Restyle()
    {
        var p = Theme.Palette;

        BackColor = p.Background;
        _body.BackColor = p.Background;
        _main.BackColor = p.Background;
        _host.BackColor = p.Background;

        if (_themeButton is not null)
        {
            string wanted = Theme.Mode == ThemeMode.Dark ? "Light theme" : "Dark theme";
            if (_themeButton.Text != wanted) _themeButton.Text = wanted;
        }

        _header.Invalidate();
        _sidebar.Invalidate();
        _sidebar.Refresh();
        Invalidate(true);
    }

    private void RunOnboarding()
    {
        using var wizard = new OnboardingForm(_pipeline);
        wizard.ShowDialog(this);
        _settings.OnboardingDone = true;
        _settings.Save();
        ShowPage(_home!);
    }

    // ---- behaviour ----

    public void ApplyHotkey() => _hotkeys.Register(_settings.Hotkey);

    public void ApplyCrosshairHotkey() => _crossHotkeys.Register(_settings.CrosshairHotkey);

    public void ApplyQuickHotkey() => _quickHotkeys.Register(_settings.QuickPopupHotkey);

    private QuickPopup? _popup;

    private void ShowQuickPopup()
    {
        _popup?.Close();
        _popup = new QuickPopup(this);
        _popup.FormClosed += (_, _) => _popup = null;
        _popup.ShowNearCursor();
    }

    /// <summary>Re-applies a saved image-derived palette on startup.</summary>
    private void ApplyStoredImageTheme()
    {
        if (!_settings.UseImageTheme || string.IsNullOrWhiteSpace(_settings.ThemeImagePath)) return;
        try
        {
            using var bmp = new Bitmap(_settings.ThemeImagePath);
            Theme.SetCustom(ThemeFromImage.Derive(bmp, Theme.Mode == ThemeMode.Light));
        }
        catch { /* image moved or unreadable: keep the built-in theme */ }
    }

    private void ToggleCrosshair()
    {
        if (_navOf.Keys.FirstOrDefault(p => p is CrosshairPage) is CrosshairPage page)
            page.ToggleFromHotkey();
    }

    private void CycleProfile()
    {
        var profiles = ProfileStore.Load();
        if (profiles.Count == 0) return;

        var p = profiles[_cycle % profiles.Count];
        _cycle++;

        _pipeline.Apply(p);
        var s = _settings;
        s.Vibrance = p.Vibrance; s.Saturation = p.Saturation; s.Gamma = p.Gamma;
        s.Brightness = p.Brightness; s.Contrast = p.Contrast; s.Temperature = p.Temperature;
        _tray.ShowProfile(p.Name);
        PublishPresence(p.Name);
        NotifySettingsChanged();
    }

    private async Task CheckUpdatesQuietly()
    {
        var update = await Updater.CheckAsync();
        if (update is not null) _tray.NotifyUpdate(update);
    }

    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST = 0x84;
        const int WM_MOUSEWHEEL = 0x020A;

        // WinForms forwards the wheel to the child under the pointer before any
        // OnMouseWheel runs, so claim it here using the message's own coordinates.
        if (m.Msg == WM_MOUSEWHEEL)
        {
            long lp = (long)m.LParam;
            int sx = (short)(lp & 0xFFFF);
            int sy = (short)((lp >> 16) & 0xFFFF);
            int delta = (short)((long)m.WParam >> 16);

            if (ScrollAt(PointToClient(new Point(sx, sy)), delta))
                return;
        }

        base.WndProc(ref m);
        if (m.Msg != WM_NCHITTEST || WindowState == FormWindowState.Maximized) return;

        int screenX = m.LParam.ToInt32() & 0xFFFF;
        int screenY = m.LParam.ToInt32() >> 16;
        var pos = PointToClient(new Point(screenX, screenY));

        const int edge = 6;
        bool left = pos.X <= edge;
        bool right = pos.X >= ClientSize.Width - edge;
        bool top = pos.Y <= edge;
        bool bottom = pos.Y >= ClientSize.Height - edge;

        int hit =
            top && left ? 13 : top && right ? 14 :
            bottom && left ? 16 : bottom && right ? 17 :
            left ? 10 : right ? 11 : top ? 12 : bottom ? 15 : 1;

        m.Result = (IntPtr)hit;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        SaveSettings();

        if (!_forceClose && e.CloseReason == CloseReason.UserClosing && _settings.MinimiseToTray && _tray.Visible)
        {
            e.Cancel = true;
            Hide();
            return;
        }

        _settings.Save();
        _hotkeys.Dispose();
        _crossHotkeys.Dispose();
        _quickHotkeys.Dispose();
        _audioTimer.Stop();
        _audioTimer.Dispose();
        Phone.Dispose();
        Discord.Dispose();
        _popup?.Close();
        _tray.Dispose();
        _pipeline.Reset();
        _pipeline.Dispose();
        base.OnFormClosing(e);
    }
}

internal static class Glyphs
{
    private static readonly Dictionary<string, string> Map = new()
    {
        ["Vibrance"] = "",   // brightness
        ["Crosshair"] = "",  // focus
        ["Monitor"] = "",    // TV monitor
        ["Monitor hardware"] = "", // settings/monitor
        ["Profiles"] = "",   // library/people
        ["Library"] = "",    // library
        ["Customize"] = "",  // settings sliders
        ["Settings"] = "",   // gear
    };

    public static string For(string title) => Map.TryGetValue(title, out var g) ? g : "";
}

/// <summary>Custom window chrome: drag, double-click maximise, min/max/close.</summary>
internal sealed class TitleBar : ThemedControl
{
    private readonly ShellForm _shell;
    private bool _maximised;

    public TitleBar(ShellForm shell)
    {
        _shell = shell;
        MouseDown += (_, _) => Drag();
        MouseDoubleClick += (_, _) => ToggleMax();

        var close = new TitleButton(TitleGlyph.Close) { Dock = DockStyle.Right };
        var max = new TitleButton(TitleGlyph.Maximise) { Dock = DockStyle.Right };
        var min = new TitleButton(TitleGlyph.Minimise) { Dock = DockStyle.Right };

        close.Click += (_, _) => shell.Close();
        max.Click += (_, _) => ToggleMax();
        min.Click += (_, _) => shell.WindowState = FormWindowState.Minimized;

        shell.Resize += (_, _) =>
        {
            _maximised = shell.WindowState == FormWindowState.Maximized;
            max.Maximised = _maximised;
        };

        Controls.Add(min);
        Controls.Add(max);
        Controls.Add(close);
    }

    private void ToggleMax() =>
        _shell.WindowState = _shell.WindowState == FormWindowState.Maximized
            ? FormWindowState.Normal : FormWindowState.Maximized;

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        var p = P;
        g.Clear(p.Background);
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var logo = BrandAssets.Logo;
        bool drewLogo = false;

        if (logo is not null)
        {
            try
            {
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                g.DrawImage(logo, new Rectangle(16, 10, 28, 28));
                drewLogo = true;
            }
            catch { drewLogo = false; }
        }

        if (!drewLogo)
        {
            using var dot = new SolidBrush(p.Accent);
            g.FillEllipse(dot, 20, 19, 10, 10);
        }

        Draw.Text(g, Branding.Name, new Font("Segoe UI", 9.75f, FontStyle.Bold), p.Text,
            new Rectangle(52, 0, 240, Height));

        using var pen = new Pen(p.Border);
        g.DrawLine(pen, 0, Height - 1, Width, Height - 1);
    }

    private void Drag()
    {
        ReleaseCapture();
        SendMessage(_shell.Handle, 0xA1, 0x2, 0);
    }

    [System.Runtime.InteropServices.DllImport("user32.dll")] private static extern bool ReleaseCapture();
    [System.Runtime.InteropServices.DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr h, int m, int w, int l);
}

internal enum TitleGlyph { Minimise, Maximise, Restore, Close }

internal sealed class TitleButton : ThemedControl
{
    private readonly TitleGlyph _glyph;
    private bool _hover;

    /// <summary>Switches maximise to restore while the window is maximised.</summary>
    public bool Maximised
    {
        set
        {
            if (_glyph is not (TitleGlyph.Maximise or TitleGlyph.Restore)) return;
            _restore = value;
            Invalidate();
        }
    }

    private bool _restore;

    public TitleButton(TitleGlyph glyph)
    {
        _glyph = glyph;
        Width = 46;
        Cursor = Cursors.Hand;
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        bool closing = _glyph == TitleGlyph.Close;

        // Windows caption buttons fill the whole title-bar height on hover
        if (_hover)
        {
            using var brush = new SolidBrush(closing ? Color.FromArgb(0xE8, 0x11, 0x23) : P.Hover);
            g.FillRectangle(brush, ClientRectangle);
        }

        var color = _hover && closing ? Color.White : P.Text;
        int cx = Width / 2, cy = Height / 2;

        // Fluent / MDL2 caption glyphs, the same ones Windows uses
        string glyph = (_glyph, _restore) switch
        {
            (TitleGlyph.Minimise, _) => "",
            (TitleGlyph.Close, _) => "",
            (_, true) => "",
            _ => ""
        };

        using var font = new Font("Segoe MDL2 Assets", 9f);
        Draw.Text(g, glyph, font, color, new Rectangle(0, 0, Width, Height),
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
    }
}
