namespace Radiance.Core;

/// <summary>
/// One place that turns settings into hardware writes.
/// Digital vibrance goes to the NVIDIA driver; the tone controls go to the gamma ramp.
/// </summary>
public sealed class ColorPipeline : IDisposable
{
    private readonly NvidiaVibrance _vibrance = new();
    private readonly ShaderOverlay _overlay = new();
    private bool _overlayWanted;

    // last applied state, so it can be put back after something resets it
    private ToneState _lastTone = ToneState.Neutral;
    private double[,]? _lastVision;
    private int _lastDriver = 50;
    private bool _hasApplied;

    public bool VibranceAvailable => _vibrance.IsAvailable;
    public string? VibranceError => _vibrance.LastError;
    public int DisplayCount => _vibrance.DisplayCount;
    public IReadOnlyList<int> Levels => _vibrance.ReadAll();

    public void Initialize() => _vibrance.Initialize();

    /// <summary>
    /// Applies a complete state. The driver accepts 0-100, so saturation scales the
    /// vibrance value: saturation 150 on vibrance 60 writes 90 to the driver.
    /// </summary>
    public bool ShaderAvailable => _overlay.IsReady;
    public string? ShaderError => _overlay.LastError;

    public void Apply(int vibrance, int saturation, ToneState tone,
        double[,]? vision = null, bool useShader = false)
    {
        // With the shader path the matrix does saturation and colour vision per pixel,
        // so the driver gets the plain vibrance value instead of a scaled one.
        int driver = useShader
            ? Math.Clamp(vibrance, 0, 100)
            : Math.Clamp((int)Math.Round(vibrance * (saturation / 100.0)), 0, 100);

        _lastTone = tone;
        _lastVision = vision;
        _lastDriver = driver;
        _hasApplied = true;

        _vibrance.SetLevel(driver);

        // When the shader runs it also applies the vision matrix, so the ramp must not.
        GammaRamp.Apply(tone, useShader ? null : vision);

        ApplyShader(useShader, saturation, vision);
    }

    private void ApplyShader(bool useShader, int saturation, double[,]? vision)
    {
        bool needed = useShader && (saturation != 100 || vision is not null);

        if (!needed)
        {
            if (_overlayWanted) { _overlay.Stop(); _overlayWanted = false; }
            return;
        }

        if (!_overlayWanted)
        {
            if (!_overlay.Start()) return; // fall back: ramp already applied
            _overlayWanted = true;
        }

        _overlay.SetMatrix(Matrix5x5.Build(saturation / 100f, vision));
    }

    public void Apply(AppSettings s) =>
        Apply(s.Vibrance, s.Saturation, FromSettings(s),
            ColourVision.For(s.ColourVisionMode, s.ColourVisionCorrection),
            s.UseShaderOverlay);

    /// <summary>Applies a saved look.</summary>
    public void Apply(Profile p) =>
        Apply(p.Vibrance, p.Saturation, new ToneState(
            Gamma: p.Gamma,
            Brightness: p.Brightness,
            Contrast: p.Contrast,
            Temperature: p.Temperature,
            Red: p.Red,
            Green: p.Green,
            Blue: p.Blue,
            Hue: p.Hue,
            Blacks: p.Blacks,
            Whites: p.Whites,
            Shadows: p.Shadows,
            Highlights: p.Highlights,
            ShadowTint: p.ShadowTint,
            HighlightTint: p.HighlightTint));

    public static ToneState FromSettings(AppSettings s)
    {
        var tone = RawTone(s);

        // Eye care folds into the applied tone rather than living beside it.
        if (s.EyeCareWarmth > 0)
            tone = tone with
            {
                Temperature = Math.Clamp(tone.Temperature + (int)(s.EyeCareWarmth * 0.6), -100, 100),
                Brightness = Math.Clamp(tone.Brightness - (int)(s.EyeCareWarmth * 0.15), 50, 170)
            };

        if (s.EyeCareDim > 0)
            tone = tone with { Brightness = Math.Clamp(tone.Brightness - (int)(s.EyeCareDim * 0.35), 50, 170) };

        return tone;
    }

    private static ToneState RawTone(AppSettings s) => new(
        Gamma: s.Gamma,
        Brightness: s.Brightness,
        Contrast: s.Contrast,
        Temperature: s.Temperature,
        Red: s.Red,
        Green: s.Green,
        Blue: s.Blue,
        Hue: s.Hue,
        Blacks: s.Blacks,
        Whites: s.Whites,
        Shadows: s.Shadows,
        Highlights: s.Highlights,
        ShadowTint: s.ShadowTint,
        HighlightTint: s.HighlightTint);

    /// <summary>
    /// Puts the colour state back. Windows resets the display gamma ramp when a
    /// fullscreen swap chain appears (the per-pixel overlay), and some display
    /// changes clear the driver level; calling this on a timer keeps the look
    /// applied instead of silently dropping back to default.
    /// </summary>
    public void Reassert()
    {
        if (!_hasApplied) return;

        if (_vibrance.IsAvailable) _vibrance.SetLevel(_lastDriver);
        GammaRamp.Apply(_lastTone, _overlayWanted ? null : _lastVision);
    }

    /// <summary>Returns the machine to neutral.</summary>
    public void Reset()
    {
        _hasApplied = false;
        _lastTone = ToneState.Neutral;
        _lastVision = null;
        _lastDriver = _vibrance.ReadDefault();

        if (_overlayWanted) { _overlay.Stop(); _overlayWanted = false; }
        _vibrance.SetLevel(_vibrance.ReadDefault());
        GammaRamp.Reset();
    }

    public void Dispose()
    {
        _overlay.Dispose();
        _vibrance.Dispose();
    }
}