using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace Radiance.Core;

/// <summary>
/// Screenshot that bakes the active look into the image: the 5x5 colour matrix
/// (saturation + colour vision) and the gamma-ramp tone controls are applied to
/// every pixel, so the file matches what the screen looks like.
/// </summary>
public static class ColourScreenshot
{
    public static string Folder { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.MyPictures), Branding.Name);

    public static string Capture(AppSettings settings, bool applyLook = true)
    {
        var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
        var bounds = screen.Bounds;

        using var bmp = new Bitmap(bounds.Width, bounds.Height);
        using (var g = Graphics.FromImage(bmp))
            g.CopyFromScreen(bounds.Location, Point.Empty, bounds.Size);

        if (applyLook)
            ApplyLook(bmp, settings);

        Directory.CreateDirectory(Folder);
        string file = Path.Combine(Folder, $"{Branding.Name}-{DateTime.Now:yyyy-MM-dd-HH-mm-ss}.png");
        bmp.Save(file, ImageFormat.Png);
        return file;
    }

    /// <summary>Applies the matrix, then the tone ramp, to an existing bitmap in place.</summary>
    public static void ApplyLook(Bitmap bmp, AppSettings settings)
    {
        float[] matrix = Matrix5x5.Build(settings.Saturation / 100f,
            ColourVision.For(settings.ColourVisionMode, settings.ColourVisionCorrection));

        byte[]? lut = BuildRamp(settings);

        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        try
        {
            int bytes = Math.Abs(data.Stride) * bmp.Height;
            var buffer = new byte[bytes];
            Marshal.Copy(data.Scan0, buffer, 0, bytes);

            for (int i = 0; i < bytes; i += 4)
            {
                float b = buffer[i] / 255f;       // BGRA order
                float g = buffer[i + 1] / 255f;
                float r = buffer[i + 2] / 255f;

                // rows 0..2 of the 5x5 matrix, column-major like the shader packing
                float nr = r * matrix[0] + g * matrix[1] + b * matrix[2];
                float ng = r * matrix[5] + g * matrix[6] + b * matrix[7];
                float nb = r * matrix[10] + g * matrix[11] + b * matrix[12];

                byte br = ToByte(nr), bg = ToByte(ng), bb = ToByte(nb);

                if (lut is not null)
                {
                    br = lut[br];
                    bg = lut[256 + bg];
                    bb = lut[512 + bb];
                }

                buffer[i] = bb;
                buffer[i + 1] = bg;
                buffer[i + 2] = br;
            }

            Marshal.Copy(buffer, 0, data.Scan0, bytes);
        }
        finally { bmp.UnlockBits(data); }
    }

    private static byte ToByte(float v) => (byte)Math.Clamp((int)Math.Round(v * 255f), 0, 255);

    /// <summary>A 3x256 lookup table for the tone controls, matching GammaRamp.Apply.</summary>
    private static byte[]? BuildRamp(AppSettings s)
    {
        bool neutral = s.Gamma == 100 && s.Brightness == 100 && s.Contrast == 100 &&
                       s.Temperature == 0 && s.Red == 100 && s.Green == 100 && s.Blue == 100 &&
                       s.Blacks == 0 && s.Highlights == 0;
        if (neutral) return null;

        double gamma = Math.Clamp(s.Gamma, 50, 150) / 100.0;
        double brightness = Math.Clamp(s.Brightness, 50, 170) / 100.0;
        double contrast = Math.Clamp(s.Contrast, 50, 150) / 100.0;
        double warm = Math.Clamp(s.Temperature, -100, 100) / 100.0;
        double gainR = Math.Clamp(s.Red, 50, 150) / 100.0;
        double gainG = Math.Clamp(s.Green, 50, 150) / 100.0;
        double gainB = Math.Clamp(s.Blue, 50, 150) / 100.0;
        double blacks = Math.Clamp(s.Blacks, -100, 100) / 100.0;
        double highlights = Math.Clamp(s.Highlights, -100, 100) / 100.0;

        var lut = new byte[768];

        for (int i = 0; i < 256; i++)
        {
            double v = i / 255.0;
            v = Math.Pow(v, 1.0 / gamma);
            v = (v - 0.5) * contrast + 0.5;
            v *= brightness;

            if (blacks > 0) v = Math.Pow(Math.Clamp(v, 0, 1), 1 + blacks * 2);
            else if (blacks < 0) v = 1 - Math.Pow(1 - Math.Clamp(v, 0, 1), 1 - blacks * 2);

            if (highlights > 0) v = 1 - Math.Pow(1 - Math.Clamp(v, 0, 1), 1 + highlights * 2);
            else if (highlights < 0) v = Math.Pow(Math.Clamp(v, 0, 1), 1 - highlights * 2);

            v = Math.Clamp(v, 0, 1);

            double r = v * gainR, g = v * gainG, b = v * gainB;
            if (warm > 0) { r *= 1 + 0.18 * warm; b *= 1 - 0.18 * warm; }
            else if (warm < 0) { b *= 1 + 0.18 * -warm; r *= 1 - 0.18 * -warm; }

            lut[i] = ToByte((float)r);
            lut[256 + i] = ToByte((float)g);
            lut[512 + i] = ToByte((float)b);
        }

        return lut;
    }
}