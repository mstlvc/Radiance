using System.Text.Json;
using System.Text.Json.Serialization;

namespace Radiance;

/// <summary>Everything the app remembers between runs. Saved as JSON.</summary>
public sealed class AppSettings
{
    // colour
    public int Vibrance { get; set; } = 50;
    public int Saturation { get; set; } = 100;
    public int Gamma { get; set; } = 100;
    public int Brightness { get; set; } = 100;
    public int Contrast { get; set; } = 100;
    public int Temperature { get; set; }

    // colour balance
    public int Red { get; set; } = 100;
    public int Green { get; set; } = 100;
    public int Blue { get; set; } = 100;
    public int Hue { get; set; }
    public int Blacks { get; set; }
    public int Whites { get; set; }
    public int Shadows { get; set; }
    public int Highlights { get; set; }
    public int ShadowTint { get; set; }
    public int HighlightTint { get; set; }

    // colour vision
    public string ColourVisionMode { get; set; } = "normal"; // normal | red-weak | green-weak | blue-weak
    public bool ColourVisionCorrection { get; set; }
    public bool UseShaderOverlay { get; set; }

    // eye care
    public int EyeCareWarmth { get; set; }
    public int EyeCareDim { get; set; }

    // behaviour
    public bool StartWithWindows { get; set; } = true;
    public bool StartMinimised { get; set; } = false;
    public bool MinimiseToTray { get; set; } = true;
    public bool OnboardingDone { get; set; }
    public bool AutoSaveSettings { get; set; } = true;
    public string LastPage { get; set; } = "Vibrance";
    public string? LastProfileName { get; set; }

    // window position & state
    public int WindowX { get; set; } = -1;
    public int WindowY { get; set; } = -1;
    public int WindowWidth { get; set; } = 1100;
    public int WindowHeight { get; set; } = 740;
    public bool WindowMaximised { get; set; }

    public ThemeMode Theme { get; set; } = ThemeMode.Dark;
    public HotkeyConfig Hotkey { get; set; } = new();
    public HotkeyConfig CrosshairHotkey { get; set; } = new();
    public HotkeyConfig QuickPopupHotkey { get; set; } = new();

    public string? ThemeImagePath { get; set; }
    public bool UseImageTheme { get; set; }

    public bool PhoneRemoteEnabled { get; set; }
    public bool AudioLimiterEnabled { get; set; }
    public int AudioLimitPercent { get; set; } = 80;

    public CrosshairSettings Crosshair { get; set; } = new();
    public List<string> FavouriteCrosshairs { get; set; } = new();
    public CustomizeSettings Customize { get; set; } = new();

    // ---- persistence ----

    private static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter() }
    };

    private static string FilePath => Path.Combine(Branding.DataFolder, "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<AppSettings>(File.ReadAllText(FilePath), Json) ?? new();
        }
        catch { /* corrupt file: fall back to defaults */ }
        return new AppSettings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Branding.DataFolder);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this, Json));
        }
        catch { /* never block the UI on a settings write */ }
    }
}

public sealed class CrosshairSettings
{
    public bool Enabled { get; set; }
    public string Shape { get; set; } = "cross";   // cross | dot | circle | t | x
    public int Length { get; set; } = 10;
    public int Thickness { get; set; } = 2;
    public int Gap { get; set; } = 6;
    public int Opacity { get; set; } = 100;
    public bool Dot { get; set; } = true;
    public bool Outline { get; set; }
    public int OutlineThickness { get; set; } = 1;
    public bool Diagonal { get; set; }
    public bool ArmTop { get; set; } = true;
    public bool ArmBottom { get; set; } = true;
    public bool ArmLeft { get; set; } = true;
    public bool ArmRight { get; set; } = true;
    public int ColourArgb { get; set; } = Color.Lime.ToArgb();
}

public sealed class CustomizeSettings
{
    public bool TaskbarLeftAlignment { get; set; }
    public bool ShowWidgets { get; set; } = true;
    public bool ShowTaskView { get; set; } = true;
    public bool HideSearchBox { get; set; }
    public bool ShowFileExtensions { get; set; } = true;
    public bool ShowHiddenFiles { get; set; }
}

/// <summary>Global hotkey.</summary>
public sealed class HotkeyConfig
{
    public bool Enabled { get; set; }
    public Keys Key { get; set; } = Keys.None;
    public bool Ctrl { get; set; }
    public bool Alt { get; set; }
    public bool Shift { get; set; }

    [JsonIgnore]
    public string Display
    {
        get
        {
            if (!Enabled || Key == Keys.None) return "Not set";
            var parts = new List<string>();
            if (Ctrl) parts.Add("Ctrl");
            if (Alt) parts.Add("Alt");
            if (Shift) parts.Add("Shift");
            parts.Add(Key.ToString());
            return string.Join(" + ", parts);
        }
    }
}
