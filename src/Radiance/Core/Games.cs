using System.Diagnostics;
using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace Radiance.Core;

public sealed record Game(string Name, string Executable, string Source)
{
    public override string ToString() => Name;
}

/// <summary>
/// Finds installed games from the Steam and Epic launchers.
/// No network access: everything is read from local manifests.
/// </summary>
public static class Games
{
    public static IReadOnlyList<Game> Detect()
    {
        var found = new List<Game>();
        try { found.AddRange(FromSteam()); } catch { }
        try { found.AddRange(FromEpic()); } catch { }

        return found
            .Where(g => !string.IsNullOrWhiteSpace(g.Name))
            .GroupBy(g => g.Name, StringComparer.OrdinalIgnoreCase)
            .Select(g => g.First())
            .OrderBy(g => g.Name, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    // ---- Steam ----

    private static IEnumerable<Game> FromSteam()
    {
        string? steam = Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string;
        if (string.IsNullOrWhiteSpace(steam)) yield break;

        var roots = new List<string> { Path.Combine(steam, "steamapps") };
        string libraries = Path.Combine(steam, "steamapps", "libraryfolders.vdf");

        if (File.Exists(libraries))
        {
            foreach (Match m in Regex.Matches(File.ReadAllText(libraries), "\"path\"\\s+\"([^\"]+)\""))
            {
                string p = m.Groups[1].Value.Replace("\\\\", "\\");
                string apps = Path.Combine(p, "steamapps");
                if (Directory.Exists(apps) && !roots.Contains(apps, StringComparer.OrdinalIgnoreCase))
                    roots.Add(apps);
            }
        }

        foreach (var root in roots)
        {
            if (!Directory.Exists(root)) continue;

            foreach (var acf in Directory.EnumerateFiles(root, "appmanifest_*.acf"))
            {
                string text;
                try { text = File.ReadAllText(acf); } catch { continue; }

                string name = Grab(text, "name");
                string dir = Grab(text, "installdir");
                if (name.Length == 0 || dir.Length == 0) continue;

                string full = Path.Combine(root, "common", dir);
                yield return new Game(name, full, "Steam");
            }
        }
    }

    private static string Grab(string text, string key)
    {
        var m = Regex.Match(text, $"\"{Regex.Escape(key)}\"\\s+\"([^\"]*)\"");
        return m.Success ? m.Groups[1].Value : "";
    }

    // ---- Epic ----

    private static IEnumerable<Game> FromEpic()
    {
        string manifests = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            "Epic", "EpicGamesLauncher", "Data", "Manifests");

        if (!Directory.Exists(manifests)) yield break;

        foreach (var item in Directory.EnumerateFiles(manifests, "*.item"))
        {
            Game? game = null;
            try
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(item));
                var root = doc.RootElement;

                string name = root.TryGetProperty("DisplayName", out var n) ? n.GetString() ?? "" : "";
                string location = root.TryGetProperty("InstallLocation", out var l) ? l.GetString() ?? "" : "";

                if (name.Length > 0 && location.Length > 0)
                    game = new Game(name, location, "Epic");
            }
            catch { }

            if (game is not null) yield return game;
        }
    }

    /// <summary>True when the game's folder contains a running executable.</summary>
    public static bool IsRunning(Game game)
    {
        if (string.IsNullOrWhiteSpace(game.Executable)) return false;

        // Steam/Epic store the install folder; match any process whose path is under it.
        foreach (var p in Process.GetProcesses())
        {
            try
            {
                string? path = p.MainModule?.FileName;
                if (path is not null && path.StartsWith(game.Executable, StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            catch { }
            finally { p.Dispose(); }
        }
        return false;
    }
}

/// <summary>Maps a game name to the profile that should apply while it runs.</summary>
public static class GameProfileStore
{
    private static string FilePath => Path.Combine(Branding.DataFolder, "games.json");

    public static Dictionary<string, string> Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(FilePath)) ?? new();
        }
        catch { }
        return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
    }

    public static void Save(Dictionary<string, string> map)
    {
        try
        {
            Directory.CreateDirectory(Branding.DataFolder);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(map, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }
}
