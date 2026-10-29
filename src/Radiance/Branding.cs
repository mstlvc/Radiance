namespace Radiance;

/// <summary>
/// Single place to rename the app. Change these and every surface follows:
/// window titles, tray tooltip, settings folder, user agent, update feed.
/// </summary>
public static class Branding
{
    public const string Name = "Radiance";
    public const string Tagline = "Digital vibrance, colour and display control for Windows.";
    public const string Version = "1.0.0";

    /// <summary>Discord application used for rich presence. Baked in, not user-configurable.</summary>
    public const string DiscordAppId = "1565141815106084934";

    /// <summary>GitHub repo used by the update checker, e.g. "mstlvc/Radiance".</summary>
    public const string UpdateRepo = "mstlvc/Radiance";

    /// <summary>Folder under %APPDATA% for settings and profiles.</summary>
    public static string DataFolder { get; } =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), Name);
}
