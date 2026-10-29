using System.Runtime.InteropServices;

namespace Radiance.Core;

/// <summary>
/// Hardware monitor control over DDC/CI (dxva2.dll): brightness, contrast,
/// per-channel gain and input source, as opposed to the GPU-side gamma ramp.
/// Not every monitor or connection supports it; every call reports failure.
/// </summary>
public static unsafe class MonitorHardware
{
    private const byte VcpBrightness = 0x10;
    private const byte VcpContrast = 0x12;
    private const byte VcpRedGain = 0x16;
    private const byte VcpGreenGain = 0x18;
    private const byte VcpBlueGain = 0x1A;
    private const byte VcpInputSource = 0x60;

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }

    // Inline wide string: marshalling it as a managed string in an [Out] array
    // silently returned a null handle, so use a fixed buffer and read it ourselves.
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private unsafe struct PHYSICAL_MONITOR
    {
        public IntPtr Handle;
        public fixed char Description[128];
    }

    private delegate bool MonitorEnumProc(IntPtr monitor, IntPtr hdc, ref RECT rect, IntPtr data);

    [DllImport("user32.dll")]
    private static extern bool EnumDisplayMonitors(IntPtr hdc, IntPtr clip, MonitorEnumProc proc, IntPtr data);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool GetNumberOfPhysicalMonitorsFromHMONITOR(IntPtr monitor, ref uint count);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static unsafe extern bool GetPhysicalMonitorsFromHMONITOR(IntPtr monitor, uint count,
        PHYSICAL_MONITOR* monitors);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool DestroyPhysicalMonitor(IntPtr handle);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool GetMonitorBrightness(IntPtr h, ref uint min, ref uint cur, ref uint max);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool SetMonitorBrightness(IntPtr h, uint value);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool GetMonitorContrast(IntPtr h, ref uint min, ref uint cur, ref uint max);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool SetMonitorContrast(IntPtr h, uint value);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool SetVCPFeature(IntPtr h, byte code, uint value);

    [DllImport("dxva2.dll", SetLastError = true)]
    private static extern bool GetVCPFeatureAndVCPFeatureReply(IntPtr h, byte code,
        IntPtr type, ref uint current, ref uint max);

    public sealed record PhysicalMonitor(IntPtr Handle, string Description);

    /// <summary>Why the last Open() found nothing, when it was an error rather than no hardware.</summary>
    public static string? LastError { get; private set; }

    private static int _hmonitors;

    /// <summary>Step-by-step record of the last Open(), for diagnostics.</summary>
    public static List<string> Trace { get; } = new();

    /// <summary>Opens every attached physical monitor. Caller must dispose.</summary>
    public static List<PhysicalMonitor> Open()
    {
        var result = new List<PhysicalMonitor>();
        LastError = null;
        _hmonitors = 0;
        Trace.Clear();

        try
        {
            EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (IntPtr mon, IntPtr hdc, ref RECT rect, IntPtr data) =>
            {
                _hmonitors++;
                Trace.Add($"hmonitor={mon}");
                uint count = 0;
                if (!GetNumberOfPhysicalMonitorsFromHMONITOR(mon, ref count))
                {
                    LastError = "GetNumberOfPhysicalMonitorsFromHMONITOR failed, Win32 error "
                        + Marshal.GetLastWin32Error();
                    return true;
                }
                Trace.Add($"count={count}");
                if (count == 0)
                {
                    LastError = "Windows says that display has no physical monitor.";
                    return true;
                }

                unsafe
                {
                    var handles = stackalloc PHYSICAL_MONITOR[(int)count];
                    if (!GetPhysicalMonitorsFromHMONITOR(mon, count, handles))
                    {
                        LastError = "GetPhysicalMonitorsFromHMONITOR failed, Win32 error "
                            + Marshal.GetLastWin32Error();
                        return true;
                    }

                    for (int i = 0; i < count; i++)
                    {
                        IntPtr handle = handles[i].Handle;
                        string description = new string((char*)handles[i].Description);

                        Trace.Add($"handle={handle} desc='{description}'");
                        if (handle != IntPtr.Zero)
                            result.Add(new PhysicalMonitor(handle, description));
                    }
                }

                return true;
            }, IntPtr.Zero);
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
        }

        if (result.Count == 0 && LastError is null)
            LastError = $"Windows reported {_hmonitors} display(s); dxva2 opened no physical monitor.";

        return result;
    }

    public static void Close(IEnumerable<PhysicalMonitor> monitors)
    {
        foreach (var m in monitors)
            try { DestroyPhysicalMonitor(m.Handle); } catch { }
    }

    public static bool Supports(PhysicalMonitor m)
    {
        uint cur = 0, max = 0;
        return GetVCPFeatureAndVCPFeatureReply(m.Handle, VcpBrightness, IntPtr.Zero, ref cur, ref max)
               || GetMonitorBrightness(m.Handle, ref cur, ref cur, ref max);
    }

    public static (int Min, int Cur, int Max)? ReadBrightness(PhysicalMonitor m)
    {
        uint min = 0, cur = 0, max = 0;
        if (!GetMonitorBrightness(m.Handle, ref min, ref cur, ref max)) return null;
        return ((int)min, (int)cur, (int)max);
    }

    public static bool WriteBrightness(PhysicalMonitor m, int value)
    {
        var range = ReadBrightness(m);
        if (range is null) return false;
        uint v = (uint)Math.Clamp(value, range.Value.Min, range.Value.Max);
        return SetMonitorBrightness(m.Handle, v);
    }

    public static (int Cur, int Max)? ReadContrast(PhysicalMonitor m)
    {
        uint min = 0, cur = 0, max = 0;
        if (!GetMonitorContrast(m.Handle, ref min, ref cur, ref max)) return null;
        return ((int)cur, (int)max);
    }

    public static bool WriteContrast(PhysicalMonitor m, int value) =>
        SetMonitorContrast(m.Handle, (uint)Math.Clamp(value, 0, 100));

    public static (int Cur, int Max)? ReadGain(PhysicalMonitor m, string channel)
    {
        byte code = channel switch
        {
            "red" => VcpRedGain,
            "green" => VcpGreenGain,
            _ => VcpBlueGain
        };

        uint cur = 0, max = 0;
        return GetVCPFeatureAndVCPFeatureReply(m.Handle, code, IntPtr.Zero, ref cur, ref max)
            ? ((int)cur, (int)max)
            : null;
    }

    public static bool WriteGain(PhysicalMonitor m, string channel, int value)
    {
        byte code = channel switch
        {
            "red" => VcpRedGain,
            "green" => VcpGreenGain,
            _ => VcpBlueGain
        };
        return SetVCPFeature(m.Handle, code, (uint)Math.Clamp(value, 0, 100));
    }

    public static (int Cur, int Max)? ReadInput(PhysicalMonitor m)
    {
        uint cur = 0, max = 0;
        return GetVCPFeatureAndVCPFeatureReply(m.Handle, VcpInputSource, IntPtr.Zero, ref cur, ref max)
            ? ((int)cur, (int)max)
            : null;
    }

    public static bool WriteInput(PhysicalMonitor m, int source) =>
        SetVCPFeature(m.Handle, VcpInputSource, (uint)source);

    /// <summary>Common VESA input-source codes.</summary>
    public static readonly (int Code, string Name)[] InputSources =
    {
        (0x01, "Analog 1"), (0x03, "DVI 1"), (0x0F, "DisplayPort 1"),
        (0x10, "DisplayPort 2"), (0x11, "HDMI 1"), (0x12, "HDMI 2"),
    };
}