using NvAPIWrapper.Display;
using NvAPIWrapper.Native;
using NvAPIWrapper.Native.Display;
using NvAPIWrapper.Native.Display.Structures;
using NvAPIWrapper.Native.Exceptions;

namespace Radiance.Core;

/// <summary>
/// Custom resolutions through the NVIDIA driver (NVAPI timing override).
/// Windows' ChangeDisplaySettingsEx refuses timings the driver has not
/// enumerated; this path asks the driver to try them anyway.
/// </summary>
public static class CustomTiming
{
    private static readonly TimingOverride[] Formulas =
    {
        TimingOverride.CVTReducedBlanking,
        TimingOverride.CVT,
        TimingOverride.GTF,
        TimingOverride.Auto
    };

    private static bool Ready()
    {
        try { NvAPIWrapper.NVIDIA.Initialize(); return true; }
        catch { return false; }
    }

    public static (bool Ok, string Message) TryApply(string deviceName, int width, int height, int hz)
    {
        if (!Ready())
            return (false, "No NVIDIA driver — use the standard resolution list.");

        DisplayDevice? display;
        try { display = new DisplayDevice(deviceName); }
        catch
        {
            try { display = DisplayDevice.GetGDIPrimaryDisplayDevice(); }
            catch { return (false, "No NVIDIA display matched that monitor."); }
        }

        if (display is null) return (false, "No NVIDIA display matched that monitor.");

        Timing? timing = null;
        foreach (var formula in Formulas)
        {
            try
            {
                timing = DisplayApi.GetTiming(display.DisplayId,
                    new TimingInput((uint)width, (uint)height, hz, formula, false, 0));
                break;
            }
            catch (NVIDIAApiException) { }
        }

        if (timing is null)
            return (false, "The NVIDIA driver could not build a timing for that size.");

        try
        {
            var custom = new CustomResolution((uint)width, (uint)height, 32u, timing.Value);
            display.TrialCustomResolution(custom, hardwareModeSetOnly: false);
            display.SaveCustomResolution(isThisOutputIdOnly: false, isThisMonitorOnly: false);
            return (true, $"Custom {width} × {height} @ {hz} Hz applied through NVAPI.");
        }
        catch (NVIDIAApiException ex)
        {
            return (false, "NVIDIA refused the timing: " + ex.Message);
        }
        catch (Exception ex)
        {
            return (false, "Custom timing failed: " + ex.Message);
        }
    }

    public static bool Revert(string deviceName)
    {
        if (!Ready()) return false;
        try
        {
            var display = new DisplayDevice(deviceName);
            display.RevertCustomResolution();
            return true;
        }
        catch { return false; }
    }
}