using System.Runtime.InteropServices;

namespace Radiance.Core;

/// <summary>
/// Caps the system playback volume at a chosen maximum through the Windows
/// Core Audio API, so a spike cannot jump past it.
/// </summary>
public static class AudioLimiter
{
    private static Guid IID_IAudioEndpointVolume =
        new("5CDF2C82-841E-4546-9722-0CF74078229A");

    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    private class MMDeviceEnumerator { }

    private enum EDataFlow { Render = 0, Capture = 1, All = 2 }
    private enum ERole { Console = 0, Multimedia = 1, Communications = 2 }

    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDeviceEnumerator
    {
        [PreserveSig] int EnumAudioEndpoints(EDataFlow flow, int mask, out IntPtr devices);
        [PreserveSig] int GetDefaultAudioEndpoint(EDataFlow flow, ERole role, out IMMDevice device);
    }

    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDevice
    {
        [PreserveSig] int Activate(ref Guid iid, uint clsCtx, IntPtr activationParams,
            [MarshalAs(UnmanagedType.IUnknown)] out object iface);
    }

    [ComImport, Guid("5CDF2C82-841E-4546-9722-0CF74078229A"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IAudioEndpointVolume
    {
        [PreserveSig] int RegisterControlChangeNotify(IntPtr notify);
        [PreserveSig] int UnregisterControlChangeNotify(IntPtr notify);
        [PreserveSig] int GetChannelCount(out uint count);
        [PreserveSig] int SetMasterVolumeLevel(float level, ref Guid context);
        [PreserveSig] int SetMasterVolumeLevelScalar(float level, ref Guid context);
        [PreserveSig] int GetMasterVolumeLevel(out float level);
        [PreserveSig] int GetMasterVolumeLevelScalar(out float level);
    }

    /// <summary>Reads and writes the master volume, or null when unavailable.</summary>
    private static IAudioEndpointVolume? OpenEndpoint()
    {
        try
        {
            var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
            if (enumerator.GetDefaultAudioEndpoint(EDataFlow.Render, ERole.Multimedia, out var device) != 0)
                return null;

            var iid = IID_IAudioEndpointVolume;
            if (device.Activate(ref iid, 1 /* CLSCTX_ALL */, IntPtr.Zero, out var iface) != 0)
                return null;

            return iface as IAudioEndpointVolume;
        }
        catch { return null; }
    }

    public static float? GetVolume()
    {
        var ep = OpenEndpoint();
        if (ep is null) return null;
        return ep.GetMasterVolumeLevelScalar(out float v) == 0 ? v : null;
    }

    public static bool SetVolume(float scalar)
    {
        var ep = OpenEndpoint();
        if (ep is null) return false;
        var ctx = Guid.Empty;
        return ep.SetMasterVolumeLevelScalar(Math.Clamp(scalar, 0f, 1f), ref ctx) == 0;
    }

    /// <summary>Clamps the volume down to the limit if it is above it.</summary>
    public static bool Clamp(float limit)
    {
        var current = GetVolume();
        if (current is null) return false;
        if (current <= limit) return true;
        return SetVolume(limit);
    }
}