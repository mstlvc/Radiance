using System.Text.Json;

namespace Radiance.Core;

/// <summary>
/// Local shared-preset library: any number of ".radiance-look" or ".json"
/// files in a folder, plus import/export. No server, works offline.
/// </summary>
public static class SharedPresets
{
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    public const string Extension = ".radiance-look";

    public static string Folder { get; } = Path.Combine(Branding.DataFolder, "shared");

    public static List<Profile> Load()
    {
        var result = new List<Profile>();
        try
        {
            Directory.CreateDirectory(Folder);
            foreach (var file in Directory.EnumerateFiles(Folder))
            {
                if (TryRead(file, out var profile))
                {
                    profile.Name = Path.GetFileNameWithoutExtension(file);
                    result.Add(profile);
                }
            }
        }
        catch { }

        return result.OrderBy(p => p.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public static bool TryRead(string path, out Profile profile)
    {
        profile = new Profile();
        try
        {
            var text = File.ReadAllText(path);

            // accept a share code as well as JSON
            if (text.TrimStart().StartsWith("px", StringComparison.OrdinalIgnoreCase))
                return ShareCodes.TryDecodeLook(text.Trim(), out profile);

            var p = JsonSerializer.Deserialize<Profile>(text, Json);
            if (p is null) return false;
            profile = p;
            return true;
        }
        catch { return false; }
    }

    /// <summary>Writes a profile into the shared folder.</summary>
    public static string Export(Profile profile, string? name = null)
    {
        Directory.CreateDirectory(Folder);
        string file = Path.Combine(Folder, Slug(name ?? profile.Name) + Extension);
        File.WriteAllText(file, JsonSerializer.Serialize(profile, Json));
        return file;
    }

    public static bool Import(string sourcePath, out string message)
    {
        try
        {
            if (!TryRead(sourcePath, out var profile))
            {
                message = "That file is not a Radiance look.";
                return false;
            }

            profile.Name = Path.GetFileNameWithoutExtension(sourcePath);
            Export(profile);
            message = $"Imported “{profile.Name}”.";
            return true;
        }
        catch (Exception ex)
        {
            message = "Import failed: " + ex.Message;
            return false;
        }
    }

    private static string Slug(string name)
    {
        var chars = name.Select(c => char.IsLetterOrDigit(c) || c is '-' or '_' ? c : '-').ToArray();
        return new string(chars).Trim('-');
    }
}