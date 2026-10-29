using System.Drawing.Imaging;

namespace Radiance.Core;

/// <summary>Saves a PNG of the primary screen to Pictures\Radiance.</summary>
public static class Screenshot
{
    public static string Capture()
    {
        var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
        var bounds = screen.Bounds;

        using var bmp = new Bitmap(bounds.Width, bounds.Height);
        using (var g = Graphics.FromImage(bmp))
            g.CopyFromScreen(bounds.Location, Point.Empty, bounds.Size);

        string dir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.MyPictures), Branding.Name);
        Directory.CreateDirectory(dir);

        string file = Path.Combine(dir, $"{Branding.Name}-{DateTime.Now:yyyy-MM-dd-HH-mm-ss}.png");
        bmp.Save(file, ImageFormat.Png);
        return file;
    }
}
