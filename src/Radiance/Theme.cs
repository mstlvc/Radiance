using System.Drawing.Drawing2D;

namespace Radiance;

public enum ThemeMode { Dark, Light }

/// <summary>Colour palette for one theme.</summary>
public sealed record Palette(
    Color Background,
    Color Sidebar,
    Color Surface,
    Color SurfaceAlt,
    Color Border,
    Color Text,
    Color TextMuted,
    Color Accent,
    Color AccentText,
    Color Track,
    Color Hover)
{
    public static readonly Palette Dark = new(
        Background: Color.FromArgb(0x0F, 0x11, 0x15),
        Sidebar: Color.FromArgb(0x12, 0x15, 0x1A),
        Surface: Color.FromArgb(0x17, 0x1A, 0x21),
        SurfaceAlt: Color.FromArgb(0x1E, 0x22, 0x2A),
        Border: Color.FromArgb(0x26, 0x2B, 0x35),
        Text: Color.FromArgb(0xE6, 0xE8, 0xEC),
        TextMuted: Color.FromArgb(0x8B, 0x93, 0xA1),
        Accent: Color.FromArgb(0x5B, 0x8C, 0xFF),
        AccentText: Color.White,
        Track: Color.FromArgb(0x2A, 0x30, 0x3B),
        Hover: Color.FromArgb(0x1C, 0x21, 0x29));

    public static readonly Palette Light = new(
        Background: Color.FromArgb(0xF6, 0xF7, 0xF9),
        Sidebar: Color.FromArgb(0xEC, 0xEE, 0xF2),
        Surface: Color.White,
        SurfaceAlt: Color.FromArgb(0xF2, 0xF4, 0xF7),
        Border: Color.FromArgb(0xDD, 0xE1, 0xE7),
        Text: Color.FromArgb(0x10, 0x13, 0x18),
        TextMuted: Color.FromArgb(0x62, 0x6B, 0x7A),
        Accent: Color.FromArgb(0x3B, 0x6F, 0xE0),
        AccentText: Color.White,
        Track: Color.FromArgb(0xDC, 0xE1, 0xE8),
        Hover: Color.FromArgb(0xE4, 0xE8, 0xEE));
}

public static class Theme
{
    public static ThemeMode Mode { get; private set; } = ThemeMode.Dark;

    private static Palette? _custom;

    public static bool HasCustom => _custom is not null;

    public static Palette Palette => _custom ?? (Mode == ThemeMode.Dark ? Palette.Dark : Palette.Light);

    public static void SetCustom(Palette palette)
    {
        _custom = palette;
        Changed?.Invoke();
    }

    public static void ClearCustom()
    {
        if (_custom is null) return;
        _custom = null;
        Changed?.Invoke();
    }

    public static event Action? Changed;

    public static void Set(ThemeMode mode)
    {
        if (Mode == mode) return;
        Mode = mode;
        Changed?.Invoke();
    }

    // Shared type scale.
    public static readonly Font Title = new("Segoe UI", 20f, FontStyle.Bold);
    public static readonly Font H2 = new("Segoe UI", 11.5f, FontStyle.Bold);
    public static readonly Font Body = new("Segoe UI", 9.75f);
    public static readonly Font Small = new("Segoe UI", 8.75f);
    public static readonly Font Metric = new("Segoe UI", 15f, FontStyle.Bold);
}

public static class Draw
{
    public static GraphicsPath Rounded(Rectangle r, int radius)
    {
        var p = new GraphicsPath();
        int d = radius * 2;
        if (d <= 0) { p.AddRectangle(r); return p; }
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    public static void FillRounded(Graphics g, Rectangle r, int radius, Color color)
    {
        if (r.Width <= 0 || r.Height <= 0) return;
        using var path = Rounded(r, radius);
        using var b = new SolidBrush(color);
        g.FillPath(b, path);
    }

    public static void StrokeRounded(Graphics g, Rectangle r, int radius, Color color, float width = 1f)
    {
        if (r.Width <= 0 || r.Height <= 0) return;
        using var path = Rounded(r.Deflate((int)Math.Ceiling(width)), radius);
        using var pen = new Pen(color, width);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.DrawPath(pen, path);
    }

    public static Rectangle Deflate(this Rectangle r, int n) =>
        new(r.X + n, r.Y + n, Math.Max(0, r.Width - n * 2), Math.Max(0, r.Height - n * 2));

    /// <summary>Text trimmed to fit, with an ellipsis.</summary>
    public static void Text(Graphics g, string text, Font font, Color color, Rectangle bounds,
        TextFormatFlags flags = TextFormatFlags.Left | TextFormatFlags.VerticalCenter)
    {
        if (string.IsNullOrEmpty(text)) return;
        if (bounds.Width <= 0 || bounds.Height <= 0) return;

        bounds.Width = Math.Min(bounds.Width, 10000);
        bounds.Height = Math.Min(bounds.Height, 10000);

        TextRenderer.DrawText(g, text, font, bounds, color,
            flags | TextFormatFlags.NoPrefix | TextFormatFlags.EndEllipsis);
    }
}
