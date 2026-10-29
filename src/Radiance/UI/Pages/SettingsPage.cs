using Microsoft.Win32;
using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class SettingsPage : Page
{
    private readonly ShellForm _shell;
    private readonly ToggleRow _light = new() { Text = "Light theme" };
    private readonly ToggleRow _rememberSettings = new() { Text = "Remember all settings on exit" };
    private readonly ToggleRow _tray = new() { Text = "Close button minimises to tray" };
    private readonly ToggleRow _startup = new() { Text = "Start with Windows" };
    private readonly ToggleRow _startMinimised = new() { Text = "Start minimised to tray" };
    private readonly ToggleRow _audioLimit = new() { Text = "Limit system volume" };
    private readonly SliderRow _audioLimitValue = new() { Label = "Maximum volume", Min = 10, Max = 100 };
    private readonly ToggleRow _imageTheme = new() { Text = "Theme from an image" };
    private readonly ToggleRow _phone = new() { Text = "Phone remote (local network)" };
    private readonly Caption _discordStatus = new();
    private readonly Caption _phoneUrl = new();

    private readonly HotkeyPicker _hotkeyProfiles = new();
    private readonly HotkeyPicker _hotkeyCrosshair = new();
    private readonly HotkeyPicker _hotkeyPopup = new();
    private readonly Caption _hotkeyNote = new();
    private readonly Caption _updateStatus = new();
    private readonly Caption _saveFeedback = new();
    private bool _loading = true;

    public override string Title => "Settings";
    public override string Subtitle => "Appearance, behaviour, hotkeys and updates.";

    public SettingsPage(ShellForm shell)
    {
        _shell = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        var look = stack.Add("Appearance");
        var themeRow = new Panel { Height = 38, BackColor = Color.Transparent };
        var pickImage = new FlatButton { Text = "Choose image…", Accent = false, Width = 150, Height = 32, Top = 3 };
        var clearImage = new FlatButton { Text = "Built-in theme", Accent = false, Width = 140, Height = 32, Left = 160, Top = 3 };
        pickImage.Click += (_, _) => PickThemeImage();
        clearImage.Click += (_, _) => ClearImageTheme();
        themeRow.Controls.Add(clearImage);
        themeRow.Controls.Add(pickImage);
        look.AddAll(_light, _imageTheme, themeRow);

        var behaviour = stack.Add("Behaviour");
        var saveRow = new Panel { Height = 36, BackColor = Color.Transparent };
        var saveNow = new FlatButton { Text = "Save settings now", Accent = false, Width = 150, Height = 30, Top = 3 };
        saveNow.Click += (_, _) =>
        {
            _shell.SaveSettings();
            _saveFeedback.Text = "Settings saved successfully.";
            _saveFeedback.Invalidate();
        };
        _saveFeedback.Left = 162;
        _saveFeedback.Top = 9;
        _saveFeedback.Width = 350;
        saveRow.Controls.Add(saveNow);
        saveRow.Controls.Add(_saveFeedback);

        behaviour.AddAll(_startup, _startMinimised, _tray, _rememberSettings, _audioLimit, _audioLimitValue, saveRow);

        var hotkeys = stack.Add("Hotkeys", "Click a field, then press a combination. Esc cancels.");
        hotkeys.AddAll(
            new Caption { Text = "Cycle profiles" }, _hotkeyProfiles,
            new Caption { Text = "Toggle crosshair" }, _hotkeyCrosshair,
            new Caption { Text = "Quick vibrance popup" }, _hotkeyPopup,
            _hotkeyNote);

        var connected = stack.Add("Phone and Discord",
            "Both stay on this machine: a local web page, and the Discord client's own pipe.");

        var phoneRow = new Panel { Height = 36, BackColor = Color.Transparent };
        _phoneUrl.Left = 0;
        _phoneUrl.Top = 8;
        _phoneUrl.Width = 640;
        phoneRow.Controls.Add(_phoneUrl);

        var discordRow = new Panel { Height = 36, BackColor = Color.Transparent };
        var reconnectDiscord = new FlatButton { Text = "Reconnect Discord", Accent = false, Width = 150, Height = 30, Top = 3 };
        reconnectDiscord.Click += (_, _) =>
        {
            Task.Run(() =>
            {
                _shell.ApplyDiscord();
                try { Invoke(LoadState); } catch { }
            });
        };
        discordRow.Controls.Add(reconnectDiscord);

        connected.AddAll(_phone, phoneRow, _discordStatus, discordRow);

        var support = stack.Add("Diagnostics", "Saves a zip to your Desktop. Nothing is uploaded.");
        var reportRow = new Panel { Height = 38, BackColor = Color.Transparent };
        var report = new FlatButton { Text = "Save diagnostics report", Accent = false, Width = 210, Height = 32, Top = 3 };
        report.Click += (_, _) => SaveReport();
        reportRow.Controls.Add(report);
        support.Add(reportRow);

        var updates = stack.Add("Updates");
        var check = new FlatButton { Text = "Check for updates", Width = 160, Height = 34 };
        check.Click += async (_, _) => await CheckUpdates();
        var rowUpdate = new Panel { Height = 40, BackColor = Color.Transparent };
        _updateStatus.Left = 172;
        _updateStatus.Top = 10;
        _updateStatus.Width = 420;
        rowUpdate.Controls.Add(_updateStatus);
        rowUpdate.Controls.Add(check);
        updates.Add(rowUpdate);

        var about = stack.Add("About");
        about.AddAll(
            new Caption { Text = $"{Branding.Name} {Branding.Version} — {Branding.Tagline}" },
            new Caption { Text = $"Settings: {Branding.DataFolder}" });

        var rowLinks = new Panel { Height = 38, BackColor = Color.Transparent };
        var reset = new FlatButton { Text = "Reset all settings", Accent = false, Width = 160, Height = 32 };
        var folder = new FlatButton { Text = "Open settings folder", Accent = false, Width = 170, Height = 32, Left = 172 };
        reset.Click += (_, _) => ResetAll();
        folder.Click += (_, _) => OpenFolder();
        rowLinks.Controls.Add(folder);
        rowLinks.Controls.Add(reset);
        about.Add(rowLinks);

        _light.CheckedChanged += (_, _) => ApplyTheme();
        _rememberSettings.CheckedChanged += (_, _) =>
        {
            if (_loading) return;
            _shell.Settings.AutoSaveSettings = _rememberSettings.Checked;
            _shell.SaveSettings();
        };
        _tray.CheckedChanged += (_, _) => { _shell.Settings.MinimiseToTray = _tray.Checked; _shell.SaveSettings(); };
        _startup.CheckedChanged += (_, _) =>
        {
            if (_loading) return;
            _shell.Settings.StartWithWindows = _startup.Checked;
            StartupManager.SetEnabled(_startup.Checked);
            _shell.SaveSettings();
        };
        _startMinimised.CheckedChanged += (_, _) =>
        {
            if (_loading) return;
            _shell.Settings.StartMinimised = _startMinimised.Checked;
            _shell.SaveSettings();
        };
        _hotkeyProfiles.BindingChanged += (_, _) =>
        {
            _shell.Settings.Hotkey = _hotkeyProfiles.Config;
            _shell.ApplyHotkey(); _shell.SaveSettings();
            _hotkeyNote.Text = "Profile hotkey set to " + _hotkeyProfiles.Config.Display + ".";
            _hotkeyNote.Invalidate();
        };

        _hotkeyCrosshair.BindingChanged += (_, _) =>
        {
            _shell.Settings.CrosshairHotkey = _hotkeyCrosshair.Config;
            _shell.ApplyCrosshairHotkey(); _shell.SaveSettings();
            _hotkeyNote.Text = "Crosshair hotkey set to " + _hotkeyCrosshair.Config.Display + ".";
            _hotkeyNote.Invalidate();
        };

        _hotkeyPopup.BindingChanged += (_, _) =>
        {
            _shell.Settings.QuickPopupHotkey = _hotkeyPopup.Config;
            _shell.ApplyQuickHotkey(); _shell.SaveSettings();
            _hotkeyNote.Text = "Popup hotkey set to " + _hotkeyPopup.Config.Display + ".";
            _hotkeyNote.Invalidate();
        };

        _hotkeyProfiles.Rejected += (_, why) => { _hotkeyNote.Text = why; _hotkeyNote.Invalidate(); };
        _hotkeyCrosshair.Rejected += (_, why) => { _hotkeyNote.Text = why; _hotkeyNote.Invalidate(); };
        _hotkeyPopup.Rejected += (_, why) => { _hotkeyNote.Text = why; _hotkeyNote.Invalidate(); };

        _audioLimit.CheckedChanged += (_, _) => { _shell.Settings.AudioLimiterEnabled = _audioLimit.Checked; _shell.SaveSettings(); };
        _audioLimitValue.ValueChanged += (_, _) => { _shell.Settings.AudioLimitPercent = _audioLimitValue.Value; _shell.SaveSettings(); };
        _imageTheme.CheckedChanged += (_, _) => { _shell.Settings.UseImageTheme = _imageTheme.Checked; if (!_imageTheme.Checked) ClearImageTheme(); _shell.SaveSettings(); };

        _phone.CheckedChanged += (_, _) => TogglePhone();

        LoadState();

        // The sidebar also has a theme button: keep this copy in step.
        Theme.Changed += SyncThemeToggle;
        _shell.SettingsChanged += LoadState;
    }

    private void SyncThemeToggle()
    {

        bool light = Theme.Mode == ThemeMode.Light;
        if (_light.Checked == light) return;

        _loading = true;
        _light.Checked = light;
        _loading = false;
    }

    private void LoadState()
    {
        _loading = true;
        var s = _shell.Settings;
        _light.Checked = s.Theme == ThemeMode.Light;
        _rememberSettings.Checked = s.AutoSaveSettings;
        _tray.Checked = s.MinimiseToTray;
        _startup.Checked = s.StartWithWindows;
        _startMinimised.Checked = s.StartMinimised;
        _hotkeyProfiles.Config = s.Hotkey;
        _hotkeyCrosshair.Config = s.CrosshairHotkey;
        _hotkeyPopup.Config = s.QuickPopupHotkey;
        _audioLimit.Checked = s.AudioLimiterEnabled;
        _audioLimitValue.Value = s.AudioLimitPercent;
        _imageTheme.Checked = s.UseImageTheme && !string.IsNullOrWhiteSpace(s.ThemeImagePath);
        _phone.Checked = s.PhoneRemoteEnabled;
        _discordStatus.Text = _shell.Discord.Connected
            ? $"Discord presence: connected as {_shell.Discord.UserName ?? "user"} (playing {Branding.Name})."
            : $"Discord presence: waiting for Discord desktop app ({_shell.Discord.LastError ?? "not connected"}).";
        _discordStatus.Invalidate();
        UpdatePhoneUrl();
        _loading = false;
    }

    public override void OnShown() => LoadState();

    private void ApplyTheme()
    {
        if (_loading) return;
        var mode = _light.Checked ? ThemeMode.Light : ThemeMode.Dark;
        _shell.Settings.Theme = mode;
        Theme.Set(mode);
        _shell.Settings.Save();
    }

    private async Task CheckUpdates()
    {
        _updateStatus.Text = "Checking…";
        _updateStatus.Invalidate();

        var update = await Updater.CheckAsync();
        if (update is null)
        {
            _updateStatus.Text = "You are on the latest version.";
        }
        else
        {
            _updateStatus.Text = $"{update.Version} is available — opening the release page.";
            Updater.Open(update.Url);
        }
        _updateStatus.Invalidate();
    }

    private void TogglePhone()
    {
        _shell.Settings.PhoneRemoteEnabled = _phone.Checked;

        if (_phone.Checked) _shell.StartPhone();
        else _shell.StopPhone();

        UpdatePhoneUrl();
    }

    private void UpdatePhoneUrl()
    {
        _phoneUrl.Text = _shell.Phone.Running
            ? "Open on your phone: " + _shell.Phone.Url
            : "Start the remote to get a local address.";
        _phoneUrl.Invalidate();
    }

    private void SaveReport()
    {
        try
        {
            string path = Diagnostics.CreateBundle(_shell.Settings);
            _updateStatus.Text = "Report saved to " + path;
        }
        catch (Exception ex)
        {
            _updateStatus.Text = "Could not save the report: " + ex.Message;
        }
        _updateStatus.Invalidate();
    }

    private void PickThemeImage()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Choose an image for the theme",
            Filter = "Images|*.png;*.jpg;*.jpeg;*.bmp"
        };

        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        try
        {
            using var bmp = new Bitmap(dialog.FileName);
            Theme.SetCustom(ThemeFromImage.Derive(bmp, Theme.Mode == ThemeMode.Light));
            _shell.Settings.ThemeImagePath = dialog.FileName;
            _shell.Settings.UseImageTheme = true;
            _imageTheme.Checked = true;
            _updateStatus.Text = "Theme built from the image.";
        }
        catch (Exception ex)
        {
            _updateStatus.Text = "Could not read that image: " + ex.Message;
        }
        _updateStatus.Invalidate();
    }

    private void ClearImageTheme()
    {
        Theme.ClearCustom();
        _shell.Settings.UseImageTheme = false;
        _shell.Settings.ThemeImagePath = null;
        _imageTheme.Checked = false;
    }

    private void ResetAll()
    {
        var s = _shell.Settings;
        s.Vibrance = 50; s.Saturation = 100; s.Gamma = 100;
        s.Brightness = 100; s.Contrast = 100; s.Temperature = 0;
        s.MinimiseToTray = true;
        s.StartMinimised = false;
        s.Hotkey = new HotkeyConfig();
        s.CrosshairHotkey = new HotkeyConfig();
        s.Crosshair = new CrosshairSettings();
        s.Customize = new CustomizeSettings();
        s.UseImageTheme = false; s.ThemeImagePath = null;
        s.AudioLimiterEnabled = false; s.AudioLimitPercent = 80;
        s.QuickPopupHotkey = new HotkeyConfig();
        s.PhoneRemoteEnabled = false;
        s.Theme = ThemeMode.Dark;
        s.OnboardingDone = true;
        s.Save();

        Theme.Set(ThemeMode.Dark);
        _shell.ApplyHotkey(); _shell.SaveSettings();
        _shell.ApplyCrosshairHotkey(); _shell.SaveSettings();
        _shell.Pipeline.Reset();

        LoadState();
        _updateStatus.Text = "Settings reset to defaults.";
        _updateStatus.Invalidate();
    }

    private static void OpenFolder()
    {
        try
        {
            Directory.CreateDirectory(Branding.DataFolder);
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(Branding.DataFolder)
            {
                UseShellExecute = true
            });
        }
        catch { }
    }

    // ---- start with Windows ----

    private static bool StartupEnabled() => StartupManager.IsEnabled();
    private static void SetStartup(bool on) => StartupManager.SetEnabled(on);

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            Theme.Changed -= SyncThemeToggle;
            _shell.SettingsChanged -= LoadState;
        }
        base.Dispose(disposing);
    }
}
