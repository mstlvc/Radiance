using System.Runtime.InteropServices;

namespace Radiance.Core;

/// <summary>
/// Per-display gamma ramp: brightness, contrast, gamma curve and colour temperature.
/// Works on any GPU; the driver may reject it on HDR displays.
/// </summary>
public static class GammaRamp
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

    [StructLayout(LayoutKind.Sequential)]
    private struct RAMP
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)] public ushort[] Red;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)] public ushort[] Green;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)] public ushort[] Blue;
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern bool EnumDisplayDevices(string? device, uint devNum, ref DISPLAY_DEVICE info, uint flags);

    [DllImport("gdi32.dll")] private static extern IntPtr CreateDC(string driver, string? device, string? port, IntPtr mode);
    [DllImport("gdi32.dll")] private static extern bool DeleteDC(IntPtr dc);
    [DllImport("gdi32.dll")] private static extern bool SetDeviceGammaRamp(IntPtr dc, ref RAMP ramp);
    [DllImport("gdi32.dll")] private static extern bool GetDeviceGammaRamp(IntPtr dc, ref RAMP ramp);

    private const int DISPLAY_DEVICE_ATTACHED_TO_DESKTOP = 0x1;

    public static IReadOnlyList<string> AttachedDeviceNames()
    {
        var list = new List<string>();
        var d = new DISPLAY_DEVICE { cb = Marshal.SizeOf<DISPLAY_DEVICE>() };
        for (uint i = 0; EnumDisplayDevices(null, i, ref d, 0); i++)
        {
            if ((d.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0)
                list.Add(d.DeviceName);
            d.cb = Marshal.SizeOf<DISPLAY_DEVICE>();
        }
        return list;
    }

    /// <summary>
    /// Builds and applies a 256-entry ramp from the full tone state:
    /// gamma, brightness, contrast, temperature, per-channel gain,
    /// blacks and highlights.
    /// </summary>
    public static bool Apply(ToneState tone, double[,]? vision = null)
    {
        double gamma = Math.Clamp(tone.Gamma, 50, 150) / 100.0;
        double brightness = Math.Clamp(tone.Brightness, 50, 170) / 100.0;
        double contrast = Math.Clamp(tone.Contrast, 50, 150) / 100.0;
        double warm = Math.Clamp(tone.Temperature, -100, 100) / 100.0;

        double gainR = Math.Clamp(tone.Red, 50, 150) / 100.0;
        double gainG = Math.Clamp(tone.Green, 50, 150) / 100.0;
        double gainB = Math.Clamp(tone.Blue, 50, 150) / 100.0;

        double blacks = Math.Clamp(tone.Blacks, -100, 100) / 100.0;
        double whites = Math.Clamp(tone.Whites, -100, 100) / 100.0;
        double shadows = Math.Clamp(tone.Shadows, -100, 100) / 100.0;
        double highlights = Math.Clamp(tone.Highlights, -100, 100) / 100.0;
        double shadowTint = Math.Clamp(tone.ShadowTint, -100, 100) / 100.0;
        double highlightTint = Math.Clamp(tone.HighlightTint, -100, 100) / 100.0;

        // hue rotation matrix (degrees), applied before the per-channel gains
        var hue = HueMatrix(Math.Clamp(tone.Hue, -180, 180));

        var ramp = new RAMP { Red = new ushort[256], Green = new ushort[256], Blue = new ushort[256] };

        double vr = 1, vg = 1, vb = 1;
        double crossRG = 0, crossRB = 0, crossGR = 0, crossGB = 0, crossBR = 0, crossBG = 0;

        if (vision is not null)
        {
            vr = vision[0, 0]; crossRG = vision[0, 1]; crossRB = vision[0, 2];
            crossGR = vision[1, 0]; vg = vision[1, 1]; crossGB = vision[1, 2];
            crossBR = vision[2, 0]; crossBG = vision[2, 1]; vb = vision[2, 2];
        }

        for (int i = 0; i < 256; i++)
        {
            double v = i / 255.0;

            v = Math.Pow(v, 1.0 / gamma);
            v = (v - 0.5) * contrast + 0.5;
            v *= brightness;

            // blacks: where black sits
            if (blacks > 0) v = Math.Pow(Math.Clamp(v, 0, 1), 1 + blacks * 2);
            else if (blacks < 0) v = 1 - Math.Pow(1 - Math.Clamp(v, 0, 1), 1 - blacks * 2);

            // whites: where white sits
            if (whites > 0) v = 1 - Math.Pow(1 - Math.Clamp(v, 0, 1), 1 + whites * 2);
            else if (whites < 0) v = Math.Pow(Math.Clamp(v, 0, 1), 1 - whites * 2);

            // shadows: lift or deepen the dark end only
            if (shadows != 0)
            {
                double weight = Math.Pow(1 - Math.Clamp(v, 0, 1), 2);
                v += shadows * 0.35 * weight;
            }

            // highlights: push or roll off the bright end only
            if (highlights != 0)
            {
                double weight = Math.Pow(Math.Clamp(v, 0, 1), 2);
                v += highlights * 0.35 * weight;
            }

            v = Math.Clamp(v, 0, 1);

            double r = v * gainR, g = v * gainG, b = v * gainB;

            // hue rotation
            if (tone.Hue != 0)
            {
                double nr = hue[0, 0] * r + hue[0, 1] * g + hue[0, 2] * b;
                double ng = hue[1, 0] * r + hue[1, 1] * g + hue[1, 2] * b;
                double nb = hue[2, 0] * r + hue[2, 1] * g + hue[2, 2] * b;
                r = nr; g = ng; b = nb;
            }

            if (warm > 0) { r *= 1 + 0.18 * warm; b *= 1 - 0.18 * warm; }
            else if (warm < 0) { b *= 1 + 0.18 * -warm; r *= 1 - 0.18 * -warm; }

            // tints: warm/cool in the shadows or the highlights only
            if (shadowTint != 0)
            {
                double w = Math.Pow(1 - Math.Clamp(v, 0, 1), 2);
                r += shadowTint * 0.12 * w;
                b -= shadowTint * 0.12 * w;
            }

            if (highlightTint != 0)
            {
                double w = Math.Pow(Math.Clamp(v, 0, 1), 2);
                r += highlightTint * 0.12 * w;
                b -= highlightTint * 0.12 * w;
            }

            r = Math.Clamp(r, 0, 1);
            g = Math.Clamp(g, 0, 1);
            b = Math.Clamp(b, 0, 1);

            if (vision is not null)
            {
                double nr = vr * r + crossRG * g + crossRB * b;
                double ng = crossGR * r + vg * g + crossGB * b;
                double nb = crossBR * r + crossBG * g + vb * b;
                r = Math.Clamp(nr, 0, 1);
                g = Math.Clamp(ng, 0, 1);
                b = Math.Clamp(nb, 0, 1);
            }

            ramp.Red[i] = (ushort)(Math.Clamp(r, 0, 1) * 65535);
            ramp.Green[i] = (ushort)(Math.Clamp(g, 0, 1) * 65535);
            ramp.Blue[i] = (ushort)(Math.Clamp(b, 0, 1) * 65535);
        }

        bool any = false;
        foreach (var name in AttachedDeviceNames())
        {
            IntPtr dc = CreateDC(name, null, null, IntPtr.Zero);
            if (dc == IntPtr.Zero) continue;
            try { any |= SetDeviceGammaRamp(dc, ref ramp); }
            finally { DeleteDC(dc); }
        }
        return any;
    }

    /// <summary>Standard hue-rotation matrix, in degrees.</summary>
    private static double[,] HueMatrix(double degrees)
    {
        double a = degrees * Math.PI / 180.0;
        double c = Math.Cos(a), s = Math.Sin(a);

        const double lr = 0.213, lg = 0.715, lb = 0.072;

        return new[,]
        {
            { lr + c * (1 - lr) + s * -lr,      lg + c * -lg + s * -lg,      lb + c * -lb + s * (1 - lb) },
            { lr + c * -lr + s * 0.143,         lg + c * (1 - lg) + s * 0.140, lb + c * -lb + s * -0.283 },
            { lr + c * -lr + s * -(1 - lr),     lg + c * -lg + s * lg,        lb + c * (1 - lb) + s * lb }
        };
    }

    /// <summary>How far the live ramp deviates from a straight line (0 = neutral).</summary>
    public static int ReadBackDeviation()
    {
        var names = AttachedDeviceNames();
        if (names.Count == 0) return -1;

        IntPtr dc = CreateDC(names[0], null, null, IntPtr.Zero);
        if (dc == IntPtr.Zero) return -1;

        try
        {
            var ramp = new RAMP
            {
                Red = new ushort[256],
                Green = new ushort[256],
                Blue = new ushort[256]
            };

            if (!GetDeviceGammaRamp(dc, ref ramp)) return -1;

            int worst = 0;
            for (int i = 0; i < 256; i++)
            {
                int linear = i * 257;
                worst = Math.Max(worst, Math.Abs(ramp.Red[i] - linear));
                worst = Math.Max(worst, Math.Abs(ramp.Green[i] - linear));
                worst = Math.Max(worst, Math.Abs(ramp.Blue[i] - linear));
            }
            return worst;
        }
        finally { DeleteDC(dc); }
    }

    /// <summary>Restores the neutral ramp on every display.</summary>
    public static void Reset()
    {
        var ramp = new RAMP { Red = new ushort[256], Green = new ushort[256], Blue = new ushort[256] };
        for (int i = 0; i < 256; i++)
        {
            ushort v = (ushort)(i * 257);
            ramp.Red[i] = ramp.Green[i] = ramp.Blue[i] = v;
        }

        foreach (var name in AttachedDeviceNames())
        {
            IntPtr dc = CreateDC(name, null, null, IntPtr.Zero);
            if (dc == IntPtr.Zero) continue;
            try { SetDeviceGammaRamp(dc, ref ramp); }
            finally { DeleteDC(dc); }
        }
    }
}
