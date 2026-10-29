using System.Runtime.InteropServices;

namespace Radiance.Core;

public sealed record HdrStatus(int Index, bool Supported, bool Enabled);

/// <summary>
/// Reads HDR (advanced colour) state per active display through the
/// Windows DisplayConfig API. Read-only; nothing is changed.
/// </summary>
public static class Hdr
{
    private const uint QDC_ONLY_ACTIVE_PATHS = 2;
    private const uint DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO = 9;
    private const uint DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE = 10;

    [StructLayout(LayoutKind.Sequential)]
    private struct LUID { public uint LowPart; public int HighPart; }

    [StructLayout(LayoutKind.Sequential)]
    private struct PATH_SOURCE_INFO
    {
        public LUID adapterId;
        public uint id;
        public uint modeInfoIdx;
        public uint statusFlags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct RATIONAL { public uint Numerator; public uint Denominator; }

    [StructLayout(LayoutKind.Sequential)]
    private struct PATH_TARGET_INFO
    {
        public LUID adapterId;
        public uint id;
        public uint modeInfoIdx;
        public uint outputTechnology;
        public uint rotation;
        public uint scaling;
        public RATIONAL refreshRate;
        public uint scanLineOrdering;
        public int targetAvailable;
        public uint statusFlags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PATH_INFO
    {
        public PATH_SOURCE_INFO sourceInfo;
        public PATH_TARGET_INFO targetInfo;
        public uint flags;
    }

    /// <summary>Opaque to us; declared at its real size so marshalling stays blittable.</summary>
    [StructLayout(LayoutKind.Sequential, Size = 64)]
    private struct MODE_INFO
    {
        public uint infoType;
        public uint id;
        public LUID adapterId;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DEVICE_INFO_HEADER
    {
        public uint type;
        public uint size;
        public LUID adapterId;
        public uint id;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct ADVANCED_COLOR_INFO
    {
        public DEVICE_INFO_HEADER header;
        public uint value;
        public uint colorEncoding;
        public uint bitsPerColorChannel;
    }

    [DllImport("user32.dll")]
    private static extern int GetDisplayConfigBufferSizes(uint flags, out uint numPath, out uint numMode);

    [DllImport("user32.dll")]
    private static extern int QueryDisplayConfig(uint flags, ref uint numPath, [Out] PATH_INFO[] paths,
        ref uint numMode, [Out] MODE_INFO[] modes, IntPtr currentTopologyId);

    [DllImport("user32.dll")]
    private static extern int DisplayConfigGetDeviceInfo(ref ADVANCED_COLOR_INFO info);

    [DllImport("user32.dll")]
    private static extern int DisplayConfigSetDeviceInfo(IntPtr info);

    [StructLayout(LayoutKind.Sequential)]
    private struct SET_ADVANCED_COLOR_STATE
    {
        public DEVICE_INFO_HEADER header;
        public uint value; // bit 1 = enable advanced colour
    }

    /// <summary>Turns HDR (advanced colour) on or off for one active display.</summary>
    public static bool SetEnabled(int displayIndex, bool enabled)
    {
        try
        {
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, out uint numPath, out uint numMode) != 0)
                return false;

            var paths = new PATH_INFO[numPath];
            var modes = new MODE_INFO[Math.Max(1, numMode)];
            if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, ref numPath, paths, ref numMode, modes, IntPtr.Zero) != 0)
                return false;

            if (displayIndex < 1 || displayIndex > numPath) return false;

            var state = new SET_ADVANCED_COLOR_STATE
            {
                header = new DEVICE_INFO_HEADER
                {
                    type = DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE,
                    size = (uint)Marshal.SizeOf<SET_ADVANCED_COLOR_STATE>(),
                    adapterId = paths[displayIndex - 1].targetInfo.adapterId,
                    id = paths[displayIndex - 1].targetInfo.id
                },
                value = enabled ? 1u : 0u
            };

            IntPtr buffer = Marshal.AllocHGlobal(Marshal.SizeOf<SET_ADVANCED_COLOR_STATE>());
            try
            {
                Marshal.StructureToPtr(state, buffer, false);
                return DisplayConfigSetDeviceInfo(buffer) == 0;
            }
            finally { Marshal.FreeHGlobal(buffer); }
        }
        catch { return false; }
    }

    public static IReadOnlyList<HdrStatus> Read()
    {
        var result = new List<HdrStatus>();

        try
        {
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, out uint numPath, out uint numMode) != 0)
                return result;

            var paths = new PATH_INFO[numPath];
            var modes = new MODE_INFO[Math.Max(1, numMode)];

            if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, ref numPath, paths, ref numMode, modes, IntPtr.Zero) != 0)
                return result;

            for (int i = 0; i < numPath; i++)
            {
                var info = new ADVANCED_COLOR_INFO
                {
                    header = new DEVICE_INFO_HEADER
                    {
                        type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO,
                        size = (uint)Marshal.SizeOf<ADVANCED_COLOR_INFO>(),
                        adapterId = paths[i].targetInfo.adapterId,
                        id = paths[i].targetInfo.id
                    }
                };

                if (DisplayConfigGetDeviceInfo(ref info) != 0) continue;

                bool supported = (info.value & 0x1) != 0;
                bool enabled = (info.value & 0x2) != 0;
                result.Add(new HdrStatus(i + 1, supported, enabled));
            }
        }
        catch { /* best effort */ }

        return result;
    }
}
