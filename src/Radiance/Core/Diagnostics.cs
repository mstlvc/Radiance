using System.IO.Compression;
using System.Text;

namespace Radiance.Core;

/// <summary>
/// Builds a local diagnostic bundle (system, displays, settings, log tail).
/// Nothing is uploaded — the zip lands in a folder you choose.
/// </summary>
public static class Diagnostics
{
    public static string Folder { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
        Branding.Name + " reports");

    public static string CreateBundle(AppSettings settings, string? note = null)
    {
        Directory.CreateDirectory(Folder);
        string stamp = DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss");
        string zipPath = Path.Combine(Folder, $"{Branding.Name}-report-{stamp}.zip");

        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);

        Add(zip, "system.txt", SystemInfo());
        Add(zip, "displays.txt", DisplayService.Describe());
        Add(zip, "settings.json", SafeSettings(settings));
        Add(zip, "shader.txt", ShaderInfo(settings));

        if (!string.IsNullOrWhiteSpace(note))
            Add(zip, "note.txt", note);

        return zipPath;
    }

    private static void Add(ZipArchive zip, string name, string content)
    {
        var entry = zip.CreateEntry(name, CompressionLevel.Optimal);
        using var stream = entry.Open();
        using var writer = new StreamWriter(stream, Encoding.UTF8);
        writer.Write(content);
    }

    private static string SystemInfo()
    {
        var sb = new StringBuilder();
        sb.AppendLine($"{Branding.Name} {Branding.Version}");
        sb.AppendLine($"OS          : {Environment.OSVersion}");
        sb.AppendLine($"64-bit      : {Environment.Is64BitOperatingSystem}");
        sb.AppendLine($"CPU         : {Environment.ProcessorCount} logical");
        sb.AppendLine($"Runtime     : {Environment.Version}");
        sb.AppendLine($"Machine     : {Environment.MachineName}");
        sb.AppendLine($"Time (UTC)  : {DateTime.UtcNow:O}");
        return sb.ToString();
    }

    private static string SafeSettings(AppSettings s) =>
        // Deliberately no hotkeys or file paths: they can identify the machine.
        $"""
        Vibrance      {s.Vibrance}
        Saturation    {s.Saturation}
        Gamma         {s.Gamma}
        Brightness    {s.Brightness}
        Contrast      {s.Contrast}
        Temperature   {s.Temperature}
        Red/Green/Blue {s.Red}/{s.Green}/{s.Blue}
        Blacks        {s.Blacks}
        Highlights    {s.Highlights}
        Colour vision {s.ColourVisionMode} (correct={s.ColourVisionCorrection})
        Shader        {s.UseShaderOverlay}
        Theme         {Theme.Mode}
        """;

    private static string ShaderInfo(AppSettings s) =>
        $"""
        Overlay requested : {s.UseShaderOverlay}
        Saturation        : {s.Saturation}
        Colour vision     : {s.ColourVisionMode}
        """;
}