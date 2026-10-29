using Microsoft.Win32;

namespace Radiance.Core;

/// <summary>
/// Windows shell tweaks (taskbar and Explorer). Writes user-scope registry values only;
/// Explorer picks most of them up on the next restart.
/// </summary>
public static class CustomizeRegistry
{
    private const string Advanced = @"Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced";

    public static void Apply(CustomizeSettings s)
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(Advanced, writable: true);
            if (key is null) return;

            // Taskbar alignment: 0 = left, 1 = centre (Windows 11)
            key.SetValue("TaskbarAl", s.TaskbarLeftAlignment ? 0 : 1, RegistryValueKind.DWord);

            // Widgets button
            key.SetValue("TaskbarDa", s.ShowWidgets ? 1 : 0, RegistryValueKind.DWord);

            // Task View button
            key.SetValue("ShowTaskViewButton", s.ShowTaskView ? 1 : 0, RegistryValueKind.DWord);

            // Search box: 0 hidden, 1 icon only, 2 search box
            key.SetValue("SearchboxTaskbarMode", s.HideSearchBox ? 1 : 2, RegistryValueKind.DWord);

            // File extensions: HideFileExt = 1 means hidden
            key.SetValue("HideFileExt", s.ShowFileExtensions ? 0 : 1, RegistryValueKind.DWord);

            // Hidden files: Hidden = 1 shows them
            key.SetValue("Hidden", s.ShowHiddenFiles ? 1 : 2, RegistryValueKind.DWord);
        }
        catch { /* registry unavailable: ignore */ }
    }

    /// <summary>Restarts Explorer so taskbar changes take effect.</summary>
    public static void RestartExplorer()
    {
        try
        {
            foreach (var p in System.Diagnostics.Process.GetProcessesByName("explorer"))
                p.Kill();

            System.Diagnostics.Process.Start("explorer.exe");
        }
        catch { }
    }
}
