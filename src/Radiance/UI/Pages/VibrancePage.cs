using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class VibrancePage : Page
{
    private readonly ShellForm _shell;

    private readonly SliderRow _vibrance = new() { Label = "Digital vibrance", Min = 0, Max = 100 };
    private readonly SliderRow _saturation = new() { Label = "Saturation", Min = 0, Max = 300 };
    private readonly SliderRow _gamma = new() { Label = "Gamma", Min = 50, Max = 150 };
    private readonly SliderRow _brightness = new() { Label = "Brightness", Min = 50, Max = 170 };
    private readonly SliderRow _contrast = new() { Label = "Contrast", Min = 50, Max = 150 };
    private readonly SliderRow _temperature = new() { Label = "Temperature", Min = -100, Max = 100 };

    private readonly SliderRow _red = new() { Label = "Red gain", Min = 50, Max = 150 };
    private readonly SliderRow _green = new() { Label = "Green gain", Min = 50, Max = 150 };
    private readonly SliderRow _blue = new() { Label = "Blue gain", Min = 50, Max = 150 };
    private readonly SliderRow _blacks = new() { Label = "Blacks", Min = -100, Max = 100 };
    private readonly SliderRow _highlights = new() { Label = "Highlights", Min = -100, Max = 100 };

    private readonly SegmentedRow _vision = new();
    private readonly ToggleRow _visionCorrect = new() { Text = "Correct instead of simulate" };
    private readonly ToggleRow _shader = new() { Text = "Per-pixel colour (D3D11 overlay)" };

    private readonly ToggleRow _eyeComfort = new() { Text = "Eye comfort (warm, dimmer)" };
    private readonly ToggleRow _nightVision = new() { Text = "Night vision (cool, brighter)" };

    private readonly Caption _status = new();
    private readonly Caption _toast = new();
    private readonly TextBox _share = new();

    private bool _loading = true;
    private bool _inComfort;
    private int? _preComfortTemp;
    private int? _preComfortBright;

    public override string Title => "Vibrance";
    public override string Subtitle => "Colour, tone, balance and vibrance for every display.";

    public VibrancePage(ShellForm shell)
    {
        _shell = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        var driver = stack.Add("Driver");
        driver.AddAll(_status, _vibrance, _saturation);

        var tone = stack.Add("Tone");
        tone.AddAll(_gamma, _brightness, _contrast, _temperature);

        var balance = stack.Add("Colour balance", "Per-channel gain, shadows and highlights.");
        balance.AddAll(_red, _green, _blue, _blacks, _highlights);

        var vision = stack.Add("Colour vision", "Simulate a deficiency, or correct for one.");
        vision.AddAll(_vision, _visionCorrect, _shader);

        var comfort = stack.Add("Comfort", "One click presets for long sessions.");
        comfort.AddAll(_eyeComfort, _nightVision);

        var share = stack.Add("Share", "Copy this look as a code, or paste one in.");
        BuildShare(share);

        var actions = stack.AddRow(44);
        var reset = new FlatButton { Text = "Reset to neutral", Accent = false, Width = 160, Height = 34 };
        var shot = new FlatButton { Text = "Screenshot", Accent = false, Width = 130, Height = 34, Left = 172 };
        reset.Click += (_, _) => ResetAll();
        shot.Click += (_, _) => TakeScreenshot();
        actions.Controls.Add(shot);
        actions.Controls.Add(reset);

        stack.AddRow(24).Controls.Add(_toast);

        _vision.SetItems(ColourVision.Modes.Select(m => (m, ShortLabel(m))));

        foreach (var s in new Control[] { _vibrance, _saturation, _gamma, _brightness, _contrast,
                                          _temperature, _red, _green, _blue, _blacks, _highlights })
            ((SliderRow)s).ValueChanged += (_, _) => ApplyLive();

        _vision.SelectionChanged += (_, _) => ApplyLive();
        _visionCorrect.CheckedChanged += (_, _) => ApplyLive();
        _shader.CheckedChanged += (_, _) => ApplyLive();

        _eyeComfort.CheckedChanged += (_, _) => ComfortChanged(warm: true, on: _eyeComfort.Checked);
        _nightVision.CheckedChanged += (_, _) => ComfortChanged(warm: false, on: _nightVision.Checked);

        LoadState();
        UpdateStatus();
        _shell.SettingsChanged += ReloadFromSettings;
    }

    private static string ShortLabel(string mode) => mode switch
    {
        "red-weak" => "Red",
        "green-weak" => "Green",
        "blue-weak" => "Blue",
        _ => "Normal"
    };

    private void BuildShare(Card host)
    {
        _share.Left = 0;
        _share.Top = 4;
        _share.Width = 360;
        _share.Height = 28;
        _share.Font = Theme.Body;
        _share.BackColor = Theme.Palette.SurfaceAlt;
        _share.ForeColor = Theme.Palette.Text;
        _share.BorderStyle = BorderStyle.FixedSingle;
        _share.PlaceholderText = "rdc-… or rdcf-…";

        var copy = new FlatButton { Text = "Copy", Accent = false, Width = 80, Height = 30, Left = 370, Top = 3 };
        var paste = new FlatButton { Text = "Apply code", Width = 110, Height = 30, Left = 458, Top = 3 };

        copy.Click += (_, _) =>
        {
            _share.Text = ShareCodes.EncodeLook(CurrentProfile());
            try { Clipboard.SetText(_share.Text); _toast.Text = "Look copied."; } catch { }
            _toast.Invalidate();
        };

        paste.Click += (_, _) => ApplyCode();

        host.AddAll(_share, copy, paste);
    }

    private void ApplyCode()
    {
        string text = _share.Text.Trim();

        Profile? look = null;
        if (ShareCodes.TryDecodeLook(text, out var single)) look = single;
        else if (ShareCodes.TryDecodeBundle(text, out var bundle)) look = bundle.Look;

        if (look is null)
        {
            _toast.Text = "That does not look like a valid code.";
            _toast.Invalidate();
            return;
        }

        _loading = true;

        _vibrance.Value = look.Vibrance;
        _saturation.Value = look.Saturation;
        _gamma.Value = look.Gamma;
        _brightness.Value = look.Brightness;
        _contrast.Value = look.Contrast;
        _temperature.Value = look.Temperature;
        _red.Value = look.Red;
        _green.Value = look.Green;
        _blue.Value = look.Blue;
        _blacks.Value = look.Blacks;
        _highlights.Value = look.Highlights;

        // fields without a slider on this page still go into the settings
        var st = _shell.Settings;
        st.Hue = look.Hue;
        st.Whites = look.Whites;
        st.Shadows = look.Shadows;
        st.ShadowTint = look.ShadowTint;
        st.HighlightTint = look.HighlightTint;
        st.ColourVisionMode = look.ColourVisionMode;
        st.ColourVisionCorrection = look.ColourVisionCorrection;
        st.EyeCareWarmth = look.EyeCareWarmth;
        st.EyeCareDim = look.EyeCareDim;
        _vision.Select(look.ColourVisionMode);
        _visionCorrect.Checked = look.ColourVisionCorrection;

        _loading = false;
        ApplyLive();
        _shell.SaveSettings();
        _toast.Text = "Look applied.";
        _toast.Invalidate();
    }

    private Profile CurrentProfile()
    {
        var s = _shell.Settings;
        return new Profile
        {
            Vibrance = s.Vibrance,
            Saturation = s.Saturation,
            Gamma = s.Gamma,
            Brightness = s.Brightness,
            Contrast = s.Contrast,
            Temperature = s.Temperature,
            Red = s.Red,
            Green = s.Green,
            Blue = s.Blue,
            Blacks = s.Blacks,
            Highlights = s.Highlights,
            Hue = s.Hue,
            Whites = s.Whites,
            Shadows = s.Shadows,
            ShadowTint = s.ShadowTint,
            HighlightTint = s.HighlightTint,
            ColourVisionMode = s.ColourVisionMode,
            ColourVisionCorrection = s.ColourVisionCorrection,
            EyeCareWarmth = s.EyeCareWarmth,
            EyeCareDim = s.EyeCareDim
        };
    }

    private void LoadState()
    {
        _loading = true;
        var s = _shell.Settings;
        _vibrance.Value = s.Vibrance;
        _saturation.Value = s.Saturation;
        _gamma.Value = s.Gamma;
        _brightness.Value = s.Brightness;
        _contrast.Value = s.Contrast;
        _temperature.Value = s.Temperature;
        _red.Value = s.Red;
        _green.Value = s.Green;
        _blue.Value = s.Blue;
        _blacks.Value = s.Blacks;
        _highlights.Value = s.Highlights;
        _vision.Select(s.ColourVisionMode);
        _visionCorrect.Checked = s.ColourVisionCorrection;
        _shader.Checked = s.UseShaderOverlay;
        _loading = false;
        ApplyLive();
    }

    public override void OnShown() => UpdateStatus();

    /// <summary>Re-reads settings after something else changed them.</summary>
    public void ReloadFromSettings()
    {
        if (IsDisposed) return;
        LoadState();
        Invalidate(true);
    }

    private void ApplyLive()
    {
        if (_loading) return;

        var s = _shell.Settings;
        s.Vibrance = _vibrance.Value;
        s.Saturation = _saturation.Value;
        s.Gamma = _gamma.Value;
        s.Brightness = _brightness.Value;
        s.Contrast = _contrast.Value;
        s.Temperature = _temperature.Value;
        s.Red = _red.Value;
        s.Green = _green.Value;
        s.Blue = _blue.Value;
        s.Blacks = _blacks.Value;
        s.Highlights = _highlights.Value;
        s.ColourVisionMode = _vision.Selected;
        s.ColourVisionCorrection = _visionCorrect.Checked;
        s.UseShaderOverlay = _shader.Checked;

        _shell.Pipeline.Apply(s);
        UpdateStatus();
        _shell.SaveSettings();
    }

    private void ResetAll()
    {
        _loading = true;
        _inComfort = true;
        _eyeComfort.Checked = false;
        _nightVision.Checked = false;
        _inComfort = false;
        _preComfortTemp = null;
        _preComfortBright = null;
        _vibrance.Value = 50;
        _saturation.Value = 100;
        _gamma.Value = 100;
        _brightness.Value = 100;
        _contrast.Value = 100;
        _temperature.Value = 0;
        _red.Value = 100;
        _green.Value = 100;
        _blue.Value = 100;
        _blacks.Value = 0;
        _highlights.Value = 0;
        _vision.Select("normal");
        _visionCorrect.Checked = false;
        _shader.Checked = false;
        _loading = false;

        _shell.Pipeline.Reset();
        ApplyLive();
        _shell.SaveSettings();
        _toast.Text = "Neutral applied.";
        _toast.Invalidate();
    }

    /// <summary>
    /// Turning a comfort mode on remembers the temperature and brightness it
    /// replaced; turning it off puts those back. Switching between the two keeps
    /// the original values rather than stacking.
    /// </summary>
    private void ComfortChanged(bool warm, bool on)
    {
        if (_inComfort || _loading) return;

        _inComfort = true;
        try
        {
            if (on)
            {
                _preComfortTemp ??= _temperature.Value;
                _preComfortBright ??= _brightness.Value;

                _eyeComfort.Checked = warm;
                _nightVision.Checked = !warm;
                _temperature.Value = warm ? 45 : -20;
                _brightness.Value = warm ? 95 : 108;

                _toast.Text = warm ? "Eye comfort on." : "Night vision on.";
            }
            else
            {
                // only undo once both switches are off
                if (!_eyeComfort.Checked && !_nightVision.Checked)
                {
                    _temperature.Value = _preComfortTemp ?? _temperature.Value;
                    _brightness.Value = _preComfortBright ?? _brightness.Value;
                    _preComfortTemp = null;
                    _preComfortBright = null;
                    _toast.Text = "Back to your previous brightness.";
                }
            }
        }
        finally { _inComfort = false; }

        ApplyLive();
        _toast.Invalidate();
    }

    private void TakeScreenshot()
    {
        try
        {
            _toast.Text = $"Saved to {ColourScreenshot.Capture(_shell.Settings)}";
        }
        catch (Exception ex)
        {
            _toast.Text = $"Screenshot failed: {ex.Message}";
        }
        _toast.Invalidate();
    }

    private void UpdateStatus()
    {
        bool ok = _shell.Pipeline.VibranceAvailable;
        _status.Text = ok
            ? $"NVIDIA driver connected · {_shell.Pipeline.DisplayCount} display(s)"
            : $"Digital vibrance unavailable — {_shell.Pipeline.VibranceError ?? "no NVIDIA GPU"}";
        _status.Ink = ok ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
        _status.Invalidate();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _shell.SettingsChanged -= ReloadFromSettings;
        base.Dispose(disposing);
    }
}