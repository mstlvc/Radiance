namespace Radiance.Core;

/// <summary>Builds a palette from an image (usually the wallpaper).</summary>
public static class ThemeFromImage
{
    public static Palette Derive(Bitmap image, bool light)
    {
        var colours = Sample(image, 4000);

        // background: the darkest common colour; surface: a little lighter
        var ordered = colours.OrderBy(kv => Luminance(kv.Key)).ToList();
        Color dark = ordered.Count > 0 ? ordered[0].Key : Color.Black;
        Color mid = ordered.Count > 0 ? ordered[ordered.Count / 2].Key : Color.Gray;

        // accent: the most saturated reasonably-bright colour
        Color accent = colours
            .Where(kv => Saturation(kv.Key) > 0.35f && Luminance(kv.Key) > 0.15f)
            .OrderByDescending(kv => Saturation(kv.Key) * kv.Value)
            .Select(kv => kv.Key)
            .FirstOrDefault(Mix(Color.FromArgb(0x5B, 0x8C, 0xFF), mid, 0.3f));

        if (light)
        {
            return new Palette(
                Background: Mix(Color.White, mid, 0.04f),
                Sidebar: Mix(Color.White, mid, 0.08f),
                Surface: Color.White,
                SurfaceAlt: Mix(Color.White, mid, 0.06f),
                Border: Mix(Color.White, dark, 0.18f),
                Text: Color.FromArgb(0x10, 0x13, 0x18),
                TextMuted: Color.FromArgb(0x62, 0x6B, 0x7A),
                Accent: accent,
                AccentText: Color.White,
                Track: Mix(Color.White, mid, 0.14f),
                Hover: Mix(Color.White, mid, 0.10f));
        }

        // A bright wallpaper would otherwise give a washed-out background,
        // so the darkest sample is deepened before it becomes the surface.
        Color seed = Mix(dark, Color.Black, Math.Max(0.72f, 1f - (float)Luminance(dark)));
        Color bg = Mix(seed, Color.Black, 0.35f);
        return new Palette(
            Background: bg,
            Sidebar: Mix(bg, Color.Black, 0.22f),
            Surface: Mix(bg, Color.White, 0.07f),
            SurfaceAlt: Mix(bg, Color.White, 0.11f),
            Border: Mix(bg, Color.White, 0.16f),
            Text: Color.FromArgb(0xE6, 0xE8, 0xEC),
            TextMuted: Color.FromArgb(0x8B, 0x93, 0xA1),
            Accent: accent,
            AccentText: Color.White,
            Track: Mix(bg, Color.White, 0.14f),
            Hover: Mix(bg, Color.White, 0.09f));
    }

    private static Dictionary<Color, int> Sample(Bitmap image, int budget)
    {
        var counts = new Dictionary<Color, int>();
        int step = Math.Max(1, (int)Math.Sqrt((double)image.Width * image.Height / budget));

        for (int y = 0; y < image.Height; y += step)
            for (int x = 0; x < image.Width; x += step)
            {
                var c = image.GetPixel(x, y);
                // quantise so similar shades merge
                var q = Color.FromArgb(c.R / 16 * 16, c.G / 16 * 16, c.B / 16 * 16);
                counts[q] = counts.TryGetValue(q, out int n) ? n + 1 : 1;
            }

        return counts;
    }

    private static double Luminance(Color c) =>
        (0.2126 * c.R + 0.7152 * c.G + 0.0722 * c.B) / 255.0;

    private static float Saturation(Color c)
    {
        int max = Math.Max(c.R, Math.Max(c.G, c.B));
        int min = Math.Min(c.R, Math.Min(c.G, c.B));
        return max == 0 ? 0 : (max - min) / (float)max;
    }

    private static Color Mix(Color a, Color b, float t) => Color.FromArgb(
        (int)(a.R + (b.R - a.R) * t),
        (int)(a.G + (b.G - a.G) * t),
        (int)(a.B + (b.B - a.B) * t));
}