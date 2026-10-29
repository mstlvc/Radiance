using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Reflection;

namespace Radiance;

/// <summary>
/// The app mark, embedded so it ships inside the single file.
///
/// GDI+ keeps a reference to the stream an Image or Icon was created from, so the
/// image is copied into memory while the stream is still open. Disposing the
/// stream first made every later use throw "Parameter is not valid".
/// </summary>
public static class BrandAssets
{
    private static Image? _logo;
    private static Icon? _icon;
    private static bool _logoFailed;
    private static bool _iconFailed;

    public static Image? Logo
    {
        get
        {
            if (_logo is not null || _logoFailed) return _logo;

            try
            {
                using var stream = Open("Radiance.Assets.radiance.png");
                if (stream is null) { _logoFailed = true; return null; }

                using var loaded = Image.FromStream(stream);

                // copy out of the stream before it closes
                var copy = new Bitmap(loaded.Width, loaded.Height, PixelFormat.Format32bppArgb);
                using (var g = Graphics.FromImage(copy))
                {
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.DrawImage(loaded, 0, 0, copy.Width, copy.Height);
                }

                _logo = copy;
            }
            catch
            {
                _logoFailed = true;
                _logo = null;
            }

            return _logo;
        }
    }

    public static Icon? AppIcon
    {
        get
        {
            if (_icon is not null || _iconFailed) return _icon;

            try
            {
                using var stream = Open("Radiance.Assets.radiance.ico");
                if (stream is null) { _iconFailed = true; return null; }

                // Icon also borrows the stream, so keep a detached copy
                using var loaded = new Icon(stream);
                _icon = (Icon)loaded.Clone();
            }
            catch
            {
                _iconFailed = true;
                _icon = null;
            }

            return _icon;
        }
    }

    private static Stream? Open(string name) =>
        Assembly.GetExecutingAssembly().GetManifestResourceStream(name);
}