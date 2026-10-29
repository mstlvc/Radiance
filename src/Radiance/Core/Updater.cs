using System.Net.Http;
using System.Text.Json;

namespace Radiance.Core;

public sealed record UpdateInfo(string Version, string Url, string Notes);

/// <summary>Checks GitHub releases for a newer build.</summary>
public static class Updater
{
    private static readonly HttpClient Http = new()
    {
        Timeout = TimeSpan.FromSeconds(10)
    };

    static Updater()
    {
        Http.DefaultRequestHeaders.UserAgent.ParseAdd($"{Branding.Name}/{Branding.Version}");
    }

    public static async Task<UpdateInfo?> CheckAsync()
    {
        try
        {
            string url = $"https://api.github.com/repos/{Branding.UpdateRepo}/releases/latest";
            using var doc = JsonDocument.Parse(await Http.GetStringAsync(url));

            string tag = doc.RootElement.GetProperty("tag_name").GetString() ?? "";
            string link = doc.RootElement.GetProperty("html_url").GetString() ?? "";
            string notes = doc.RootElement.TryGetProperty("body", out var body)
                ? body.GetString() ?? "" : "";

            string clean = tag.TrimStart('v', 'V');
            if (Version.TryParse(clean, out var remote) &&
                Version.TryParse(Branding.Version, out var local) &&
                remote > local)
            {
                return new UpdateInfo(clean, link, notes);
            }
        }
        catch { /* offline, rate-limited or repo not set: silently no update */ }
        return null;
    }

    public static void Open(string url)
    {
        try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(url) { UseShellExecute = true }); }
        catch { }
    }
}
