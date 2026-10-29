using System.Drawing.Drawing2D;

namespace Radiance.UI;

/// <summary>
/// HSV colour wheel with a brightness bar.
/// Drag inside the wheel for hue/saturation, drag the bar for value.
/// </summary>
public sealed class ColourWheel : ThemedControl
{
    private float _hue;             // 0..360
    private float _saturation = 1f; // 0..1
    private float _value = 1f;      // 0..1

    private bool _dragWheel;
    private bool _dragBar;
    private Bitmap? _wheelCache;
    private float _wheelCacheValue = -1f;

    public event EventHandler? ColourChanged;

    public Color Colour => FromHsv(_hue, _saturation, _value);

    public ColourWheel()
    {
        Height = 190;
        Cursor = Cursors.Hand;
        TabStop = true;
        AccessibleRole = AccessibleRole.Slider;
        AccessibleName = "Colour picker";
    }

    private Rectangle WheelRect
    {
        get
        {
            int side = Math.Min(Width - 8, Height - 26);
            side = Math.Max(40, side);
            int x = Math.Max(4, (Width - side) / 2);
            return new Rectangle(x, 4, side, side);
        }
    }

    private Rectangle BarRect
    {
        get
        {
            var w = WheelRect;
            int barY = w.Bottom + 6;
            return new Rectangle(4, barY, Math.Max(20, Width - 8), 14);
        }
    }

    public void SetColour(Color c, bool raise = false)
    {
        ToHsv(c, out _hue, out _saturation, out _value);
        Invalidate();
        if (raise) ColourChanged?.Invoke(this, EventArgs.Empty);
    }

    public void SetHex(string text)
    {
        if (TryParseHex(text, out var c)) SetColour(c, raise: true);
    }

    public string Hex => $"#{Colour.R:X2}{Colour.G:X2}{Colour.B:X2}";

    // ---- painting ----

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var wheel = WheelRect;
        if (wheel.Width > 0 && wheel.Height > 0)
        {
            var bmp = WheelBitmap(wheel.Width);
            if (bmp is not null) g.DrawImage(bmp, wheel);
        }

        // hue/saturation marker
        float radius = wheel.Width / 2f;
        float angle = _hue * MathF.PI / 180f;
        float dist = _saturation * radius;
        float mx = wheel.X + radius + MathF.Cos(angle) * dist;
        float my = wheel.Y + radius + MathF.Sin(angle) * dist;

        using (var outer = new Pen(Color.FromArgb(200, 0, 0, 0), 4f))
            g.DrawEllipse(outer, mx - 6, my - 6, 12, 12);
        using (var inner = new Pen(Color.White, 2f))
            g.DrawEllipse(inner, mx - 5, my - 5, 10, 10);

        // brightness bar
        var bar = BarRect;
        if (bar.Width > 0 && bar.Height > 0)
        {
            using var b = new LinearGradientBrush(
                new Rectangle(bar.X, bar.Y, bar.Width, bar.Height),
                Color.Black, FromHsv(_hue, _saturation, 1f), LinearGradientMode.Horizontal);
            using var path = Draw.Rounded(bar, 4);
            g.FillPath(b, path);

            int kx = bar.X + (int)(_value * bar.Width);
            using var knob = new SolidBrush(P.Surface);
            using var edge = new Pen(P.Text, 1.6f);
            var k = new Rectangle(kx - 4, bar.Y - 3, 8, bar.Height + 6);
            g.FillRectangle(knob, k);
            g.DrawRectangle(edge, k);
        }
    }

    private Bitmap? WheelBitmap(int size)
    {
        try
        {
            return BuildWheel(size);
        }
        catch
        {
            // never let a failed cache rebuild take the page down
            _wheelCache?.Dispose();
            _wheelCache = null;
            return null;
        }
    }

    private Bitmap? BuildWheel(int size)
    {
        if (_wheelCache is not null &&
            _wheelCache.Width == size &&
            Math.Abs(_wheelCacheValue - _value) < 0.001f)
            return _wheelCache;

        _wheelCache?.Dispose();

        // Per-pixel HSV is fast enough for a cached wheel and is exactly correct.
        var bmp = new Bitmap(size, size, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        float r = (size - 1) / 2f;

        var data = bmp.LockBits(new Rectangle(0, 0, size, size),
            System.Drawing.Imaging.ImageLockMode.WriteOnly,
            System.Drawing.Imaging.PixelFormat.Format32bppArgb);

        try
        {
            unsafe
            {
                var px = (uint*)data.Scan0;
                for (int y = 0; y < size; y++)
                {
                    float dy = y - r;
                    for (int x = 0; x < size; x++)
                    {
                        float dx = x - r;
                        float dist = MathF.Sqrt(dx * dx + dy * dy);

                        if (dist > r)
                        {
                            px[y * (data.Stride / 4) + x] = 0;
                            continue;
                        }

                        float deg = MathF.Atan2(dy, dx) * 180f / MathF.PI;
                        if (deg < 0) deg += 360f;

                        var c = FromHsv(deg, Math.Clamp(dist / r, 0f, 1f), _value);
                        px[y * (data.Stride / 4) + x] =
                            (uint)(0xFF << 24 | c.R << 16 | c.G << 8 | c.B);
                    }
                }
            }
        }
        finally
        {
            bmp.UnlockBits(data);
        }

        _wheelCache = bmp;
        _wheelCacheValue = _value;
        return bmp;
    }

    // ---- interaction ----

    protected override void OnMouseDown(MouseEventArgs e)
    {
        if (e.Button != MouseButtons.Left) return;
        Focus();

        if (WheelRect.Contains(e.Location)) { _dragWheel = true; TakeWheel(e.Location); }
        else if (BarRect.Contains(e.Location)) { _dragBar = true; TakeBar(e.X); }

        base.OnMouseDown(e);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_dragWheel) TakeWheel(e.Location);
        else if (_dragBar) TakeBar(e.X);
        base.OnMouseMove(e);
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        _dragWheel = _dragBar = false;
        base.OnMouseUp(e);
    }

    private void TakeWheel(Point p)
    {
        var w = WheelRect;
        float r = w.Width / 2f;
        float dx = p.X - (w.X + r);
        float dy = p.Y - (w.Y + r);

        float dist = MathF.Sqrt(dx * dx + dy * dy);
        _saturation = Math.Clamp(dist / r, 0f, 1f);

        if (dist > 0.5f)
        {
            float deg = MathF.Atan2(dy, dx) * 180f / MathF.PI;
            if (deg < 0) deg += 360f;
            _hue = deg;
        }

        Invalidate();
        ColourChanged?.Invoke(this, EventArgs.Empty);
    }

    private void TakeBar(int x)
    {
        var bar = BarRect;
        _value = Math.Clamp((x - bar.X) / (float)Math.Max(1, bar.Width), 0f, 1f);
        Invalidate();
        ColourChanged?.Invoke(this, EventArgs.Empty);
    }

    protected override bool IsInputKey(Keys keyData) => keyData is Keys.Left or Keys.Right || base.IsInputKey(keyData);

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Left) { _hue = (_hue - 3f + 360f) % 360f; e.Handled = true; }
        if (e.KeyCode == Keys.Right) { _hue = (_hue + 3f) % 360f; e.Handled = true; }
        if (e.Handled) { Invalidate(); ColourChanged?.Invoke(this, EventArgs.Empty); }
        base.OnKeyDown(e);
    }

    // ---- HSV helpers ----

    public static Color FromHsv(float h, float s, float v)
    {
        h = ((h % 360f) + 360f) % 360f;
        s = Math.Clamp(s, 0f, 1f);
        v = Math.Clamp(v, 0f, 1f);

        float c = v * s;
        float x = c * (1 - MathF.Abs((h / 60f) % 2 - 1));
        float m = v - c;

        (float r, float g, float b) = h switch
        {
            < 60 => (c, x, 0f),
            < 120 => (x, c, 0f),
            < 180 => (0f, c, x),
            < 240 => (0f, x, c),
            < 300 => (x, 0f, c),
            _ => (c, 0f, x)
        };

        return Color.FromArgb(
            (int)Math.Round((r + m) * 255),
            (int)Math.Round((g + m) * 255),
            (int)Math.Round((b + m) * 255));
    }

    public static void ToHsv(Color colour, out float h, out float s, out float v)
    {
        float r = colour.R / 255f, g = colour.G / 255f, b = colour.B / 255f;
        float max = Math.Max(r, Math.Max(g, b));
        float min = Math.Min(r, Math.Min(g, b));
        float delta = max - min;

        v = max;
        s = max <= 0f ? 0f : delta / max;

        if (delta <= 0f) { h = 0f; return; }

        if (max == r) h = 60f * (((g - b) / delta) % 6f);
        else if (max == g) h = 60f * ((b - r) / delta + 2f);
        else h = 60f * ((r - g) / delta + 4f);

        h = (h % 360f + 360f) % 360f;
    }

    public static string ToHex(Color c) => $"#{c.R:X2}{c.G:X2}{c.B:X2}";

    public static bool TryParseHex(string? text, out Color colour)
    {
        colour = Color.Empty;
        if (string.IsNullOrWhiteSpace(text)) return false;

        string s = text.Trim().TrimStart('#');
        if (s.Length == 3)
            s = $"{s[0]}{s[0]}{s[1]}{s[1]}{s[2]}{s[2]}";

        if (s.Length != 6) return false;
        if (!int.TryParse(s, System.Globalization.NumberStyles.HexNumber, null, out int rgb)) return false;

        colour = Color.FromArgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        return true;
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _wheelCache?.Dispose();
        base.Dispose(disposing);
    }
}
