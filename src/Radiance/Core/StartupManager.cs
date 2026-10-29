using Microsoft.Win32;

namespace Radiance.Core;

public static class StartupManager
{
    private const string RunKey = @"Software\Microsoft\Windows\CurrentVersion\Run";

    public static bool IsEnabled()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RunKey);
            return key?.GetValue(Branding.Name) is string s && s.Length > 0;
        }
        catch
        {
            return false;
        }
    }

    public static void SetEnabled(bool enable)
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RunKey, writable: true);
            if (key is null) return;

            if (enable)
            {
                string exe = Environment.ProcessPath ?? Application.ExecutablePath;
                key.SetValue(Branding.Name, $"\"{exe}\" --tray");
            }
            else
            {
                key.DeleteValue(Branding.Name, throwOnMissingValue: false);
            }
        }
        catch
        {
            // Ignore registry permissions issues
        }
    }

    public static void Sync(bool shouldRunOnStartup)
    {
        if (shouldRunOnStartup)
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(RunKey, writable: true);
                if (key is not null)
                {
                    string exe = Environment.ProcessPath ?? Application.ExecutablePath;
                    string target = $"\"{exe}\" --tray";
                    string? current = key.GetValue(Branding.Name) as string;
                    if (!string.Equals(current, target, StringComparison.OrdinalIgnoreCase))
                    {
                        key.SetValue(Branding.Name, target);
                    }
                }
            }
            catch
            {
                // Ignore registry permissions issues
            }
        }
        else if (IsEnabled())
        {
            SetEnabled(false);
        }
    }
}
