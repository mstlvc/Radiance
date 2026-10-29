using System.Runtime.InteropServices;

namespace Radiance.Core;

public sealed record DisplayInfo(string DeviceName, string FriendlyName, int Width, int Height, int RefreshHz);

public sealed record DisplayMode(int Width, int Height, int RefreshHz)
{
    public override string ToString() => $"{Width} × {Height} @ {RefreshHz} Hz";
}

/// <summary>How a non-native resolution is fitted to the panel.</summary>
public enum ScalingMode
{
    Default = 0,
    Centre = 1,
    Stretch = 2,
    Aspect = 3
}

/// <summary>Monitor enumeration, mode listing and resolution changes.</summary>
public static class DisplayService
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DISPLAY_DEVICE
    {
        public int cb;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string DeviceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceString;
        public int StateFlags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceID;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceKey;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DEVMODE
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
        public int dmFields;
        public int dmPositionX, dmPositionY;
        public int dmDisplayOrientation, dmDisplayFixedOutput;
        public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public short dmLogPixels;
        public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
        public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2;
        public int dmPanningWidth, dmPanningHeight;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern bool EnumDisplayDevices(string? device, uint devNum, ref DISPLAY_DEVICE info, uint flags);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern bool EnumDisplaySettings(string device, int modeNum, ref DEVMODE mode);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int ChangeDisplaySettingsEx(string device, ref DEVMODE mode, IntPtr hwnd, uint flags, IntPtr param);

    private const int ENUM_CURRENT_SETTINGS = -1;
    private const int ENUM_REGISTRY_SETTINGS = -2;
    private const int DISPLAY_DEVICE_ATTACHED_TO_DESKTOP = 0x1;
    private const int DM_PELSWIDTH = 0x80000, DM_PELSHEIGHT = 0x100000, DM_DISPLAYFREQUENCY = 0x400000;
    private const uint CDS_UPDATEREGISTRY = 0x1;

    public static string Describe()
    {
        var sb = new System.Text.StringBuilder();
        foreach (var d in Displays())
            sb.AppendLine($"{d.DeviceName}	{d.FriendlyName}	{d.Width}x{d.Height}@{d.RefreshHz}");

        sb.AppendLine("HDR:");
        foreach (var h in Hdr.Read())
            sb.AppendLine($"display {h.Index}: supported={h.Supported} enabled={h.Enabled}");

        return sb.ToString();
    }

    public static IReadOnlyList<DisplayInfo> Displays()
    {
        var result = new List<DisplayInfo>();
        var dev = new DISPLAY_DEVICE { cb = Marshal.SizeOf<DISPLAY_DEVICE>() };
        for (uint i = 0; EnumDisplayDevices(null, i, ref dev, 0); i++)
        {
            if ((dev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0)
            {
                var mode = new DEVMODE { dmSize = (short)Marshal.SizeOf<DEVMODE>() };
                if (EnumDisplaySettings(dev.DeviceName, ENUM_CURRENT_SETTINGS, ref mode))
                {
                    result.Add(new DisplayInfo(
                        dev.DeviceName,
                        string.IsNullOrWhiteSpace(dev.DeviceString) ? dev.DeviceName : dev.DeviceString,
                        mode.dmPelsWidth, mode.dmPelsHeight, mode.dmDisplayFrequency));
                }
            }
            dev.cb = Marshal.SizeOf<DISPLAY_DEVICE>();
        }
        return result;
    }

    /// <summary>Distinct modes the display supports, largest and fastest first.</summary>
    public static IReadOnlyList<DisplayMode> Modes(string deviceName)
    {
        var set = new HashSet<DisplayMode>();
        var mode = new DEVMODE { dmSize = (short)Marshal.SizeOf<DEVMODE>() };
        for (int i = 0; EnumDisplaySettings(deviceName, i, ref mode); i++)
        {
            if (mode.dmPelsWidth <= 0 || mode.dmPelsHeight <= 0) continue;
            int hz = mode.dmDisplayFrequency <= 1 ? 60 : mode.dmDisplayFrequency;
            set.Add(new DisplayMode(mode.dmPelsWidth, mode.dmPelsHeight, hz));
        }
        return set.OrderByDescending(m => (long)m.Width * m.Height)
                  .ThenByDescending(m => m.RefreshHz)
                  .ToList();
    }

    public static bool SetMode(string deviceName, DisplayMode target)
    {
        var mode = new DEVMODE { dmSize = (short)Marshal.SizeOf<DEVMODE>() };
        if (!EnumDisplaySettings(deviceName, ENUM_CURRENT_SETTINGS, ref mode)) return false;

        mode.dmPelsWidth = target.Width;
        mode.dmPelsHeight = target.Height;
        mode.dmDisplayFrequency = target.RefreshHz;
        mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

        int code = ChangeDisplaySettingsEx(deviceName, ref mode, IntPtr.Zero, CDS_UPDATEREGISTRY, IntPtr.Zero);
        return code == 0; // DISP_CHANGE_SUCCESSFUL
    }

    private const int DM_DISPLAYFIXEDOUTPUT = 0x20000000;

    /// <summary>Sets how a scaled resolution is fitted: stretch, aspect or centred.</summary>
    public static bool SetScaling(string deviceName, ScalingMode scaling)
    {
        var mode = new DEVMODE { dmSize = (short)Marshal.SizeOf<DEVMODE>() };
        if (!EnumDisplaySettings(deviceName, ENUM_CURRENT_SETTINGS, ref mode)) return false;

        mode.dmDisplayFixedOutput = (int)scaling;
        mode.dmFields = DM_DISPLAYFIXEDOUTPUT;

        int code = ChangeDisplaySettingsEx(deviceName, ref mode, IntPtr.Zero, CDS_UPDATEREGISTRY, IntPtr.Zero);
        return code == 0;
    }

    public static ScalingMode GetScaling(string deviceName)
    {
        var mode = new DEVMODE { dmSize = (short)Marshal.SizeOf<DEVMODE>() };
        if (!EnumDisplaySettings(deviceName, ENUM_CURRENT_SETTINGS, ref mode))
            return ScalingMode.Default;

        return (ScalingMode)mode.dmDisplayFixedOutput;
    }

    /// <summary>
    /// Tries a custom resolution. The display driver validates the timing;
    /// if it refuses, the current mode is left untouched.
    /// </summary>
    public static (bool Ok, string Message) TryCustomMode(string deviceName, int width, int height, int hz)
    {
        if (width < 320 || height < 200 || width > 16384 || height > 16384)
            return (false, "That size is out of range.");

        var target = new DisplayMode(width, height, hz);
        if (SetMode(deviceName, target))
            return (true, $"Applied {target}.");

        return (false, "The driver refused that timing. NVIDIA's control panel may still accept it as a custom resolution.");
    }
}
