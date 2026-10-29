using System.Text.Json;

namespace Radiance.Core;

/// <summary>A named look: everything the colour pipeline can restore in one step.</summary>
public sealed class Profile
{
    public string Name { get; set; } = "New profile";
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
    public string ColourVisionMode { get; set; } = "normal";
    public bool ColourVisionCorrection { get; set; }
    public int EyeCareWarmth { get; set; }
    public int EyeCareDim { get; set; }

    public Profile Clone() => (Profile)MemberwiseClone();
}

/// <summary>Loads, saves and ships the built-in profile library.</summary>
public static class ProfileStore
{
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    private static string Path_ => Path.Combine(Branding.DataFolder, "profiles.json");

    public static List<Profile> Load()
    {
        try
        {
            if (File.Exists(Path_))
                return JsonSerializer.Deserialize<List<Profile>>(File.ReadAllText(Path_)) ?? BuiltIns();
        }
        catch { }
        return BuiltIns();
    }

    public static void Save(IEnumerable<Profile> profiles)
    {
        try
        {
            Directory.CreateDirectory(Branding.DataFolder);
            File.WriteAllText(Path_, JsonSerializer.Serialize(profiles.ToList(), Json));
        }
        catch { }
    }

    /// <summary>Starting library — the same idea as the shipped game presets, our numbers.</summary>
    public static List<Profile> BuiltIns() => new()
    {
        new Profile { Name = "Neutral",        Vibrance = 50,  Saturation = 100, Gamma = 100, Brightness = 100, Contrast = 100, Temperature = 0 },
        new Profile { Name = "Competitive",    Vibrance = 65,  Saturation = 110, Gamma = 105, Brightness = 100, Contrast = 105, Temperature = -10 },
        new Profile { Name = "Vivid",          Vibrance = 80,  Saturation = 120, Gamma = 100, Brightness = 102, Contrast = 108, Temperature = 0 },
        new Profile { Name = "Night",          Vibrance = 45,  Saturation = 90,  Gamma = 95,  Brightness = 92,  Contrast = 98,  Temperature = 15 },
        new Profile { Name = "Cinematic",      Vibrance = 70,  Saturation = 105, Gamma = 102, Brightness = 100, Contrast = 110, Temperature = 25 },
        new Profile { Name = "Eye comfort",    Vibrance = 40,  Saturation = 85,  Gamma = 100, Brightness = 95,  Contrast = 95,  Temperature = 45 },
    };
}
