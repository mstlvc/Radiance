using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace Radiance.UI;

/// <summary>
/// Click-through, always-on-top crosshair drawn in the centre of the primary screen.
///
/// Uses a layered window with a per-pixel alpha bitmap. Setting Form.Opacity on a
/// window that also used TransparencyKey is what Windows rejected with
/// "Invalid parameter"; this way opacity and transparency both work.
/// </summary>
public sealed class CrosshairOverlay : Form
{
    private const int WS_EX_LAYERED = 0x80000;
    private const int WS_EX_TRANSPARENT = 0x20;
    private const int WS_EX_TOOLWINDOW = 0x80;
    private const int WS_EX_NOACTIVATE = 0x8000000;

    private const int ULW_ALPHA = 0x2;
    private const byte AC_SRC_OVER = 0x00;
    private const byte AC_SRC_ALPHA = 0x01;

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT { public int X, Y; public POINT(int x, int y) { X = x; Y = y; } }

    [StructLayout(LayoutKind.Sequential)]
    private struct SIZE { public int CX, CY; public SIZE(int x, int y) { CX = x; CY = y; } }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    private struct BLENDFUNCTION
    {
        public byte BlendOp;
        public byte BlendFlags;
        public byte SourceConstantAlpha;
        public byte AlphaFormat;
    }

    [DllImport("user32.dll")] private static extern IntPtr GetDC(IntPtr hWnd);
    [DllImport("user32.dll")] private static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
    [DllImport("user32.dll")] private static extern bool UpdateLayeredWindow(IntPtr hwnd, IntPtr hdcDst,
        ref POINT pptDst, ref SIZE psize, IntPtr hdcSrc, ref POINT pptSrc, int crKey,
        ref BLENDFUNCTION pblend, int dwFlags);
    [DllImport("gdi32.dll")] private static extern IntPtr CreateCompatibleDC(IntPtr hDC);
    [DllImport("gdi32.dll")] private static extern bool DeleteDC(IntPtr hdc);
    [DllImport("gdi32.dll")] private static extern IntPtr SelectObject(IntPtr hdc, IntPtr hObject);
    [DllImport("gdi32.dll")] private static extern bool DeleteObject(IntPtr hObject);

    public string Shape { get; set; } = "cross";
    public Color Ink { get; set; } = Color.Lime;
    public int Gap { get; set; } = 6;
    public int Length { get; set; } = 10;
    public int Thickness { get; set; } = 2;
    public bool Dot { get; set; } = true;
    public bool Outline { get; set; }
    public int OutlineThickness { get; set; } = 1;
    public bool Diagonal { get; set; }
    public bool ArmTop { get; set; } = true;
    public bool ArmBottom { get; set; } = true;
    public bool ArmLeft { get; set; } = true;
    public bool ArmRight { get; set; } = true;

    /// <summary>
    /// 0.2 to 1.0, applied to the drawing rather than the window. Named
    /// differently from Form.Opacity on purpose: that one throws when a
    /// transparency key is involved.
    /// </summary>
    public double InkOpacity { get; set; } = 1.0;

    public CrosshairOverlay()
    {
        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;

        var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
        Bounds = screen.Bounds;
    }

    protected override bool ShowWithoutActivation => true;

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
            return cp;
        }
    }

    /// <summary>Repaints the layered bitmap. Use this instead of Invalidate().</summary>
    public void Redraw()
    {
        if (!IsHandleCreated || Width <= 0 || Height <= 0) return;

        try
        {
            using var bmp = new Bitmap(Width, Height, PixelFormat.Format32bppArgb);

            using (var g = Graphics.FromImage(bmp))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.Clear(Color.Transparent);
                DrawCrosshair(g, Width / 2, Height / 2);
            }

            Push(bmp);
        }
        catch
        {
            // a hidden or closing window can refuse the update; nothing to do
        }
    }

    private void Push(Bitmap bitmap)
    {
        IntPtr screenDc = GetDC(IntPtr.Zero);
        IntPtr memDc = CreateCompatibleDC(screenDc);
        IntPtr hBitmap = IntPtr.Zero;
        IntPtr oldBitmap = IntPtr.Zero;

        try
        {
            hBitmap = bitmap.GetHbitmap(Color.FromArgb(0));
            oldBitmap = SelectObject(memDc, hBitmap);

            var size = new SIZE(bitmap.Width, bitmap.Height);
            var src = new POINT(0, 0);
            var dst = new POINT(Left, Top);

            var blend = new BLENDFUNCTION
            {
                BlendOp = AC_SRC_OVER,
                BlendFlags = 0,
                SourceConstantAlpha = 255,
                AlphaFormat = AC_SRC_ALPHA
            };

            UpdateLayeredWindow(Handle, screenDc, ref dst, ref size, memDc, ref src, 0, ref blend, ULW_ALPHA);
        }
        finally
        {
            if (oldBitmap != IntPtr.Zero) SelectObject(memDc, oldBitmap);
            if (hBitmap != IntPtr.Zero) DeleteObject(hBitmap);
            DeleteDC(memDc);
            ReleaseDC(IntPtr.Zero, screenDc);
        }
    }

    private void DrawCrosshair(Graphics g, int cx, int cy)
    {
        int t = Math.Max(1, Thickness);

        if (Outline)
        {
            using var back = new Pen(Color.FromArgb(Alpha(), 0, 0, 0), t + Math.Max(1, OutlineThickness) * 2f);
            DrawShape(g, back, cx, cy, t);
        }

        using var pen = new Pen(Color.FromArgb(Alpha(), Ink.R, Ink.G, Ink.B), t)
        {
            StartCap = LineCap.Flat,
            EndCap = LineCap.Flat
        };
        DrawShape(g, pen, cx, cy, t);
    }

    private int Alpha() => (int)Math.Clamp(InkOpacity * 255, 20, 255);

    private void DrawShape(Graphics g, Pen pen, int cx, int cy, int t)
    {
        switch (Shape)
        {
            case "dot":
                using (var b = new SolidBrush(pen.Color))
                    g.FillEllipse(b, cx - t, cy - t, t * 2, t * 2);
                break;

            case "circle":
                int r = Gap + Length;
                g.DrawEllipse(pen, cx - r, cy - r, r * 2, r * 2);
                if (Dot) DrawDot(g, pen, cx, cy, t);
                break;

            case "t":
                DrawDot(g, pen, cx, cy, t);
                g.DrawLine(pen, cx, cy + Gap, cx, cy + Gap + Length);
                g.DrawLine(pen, cx - Gap - Length, cy, cx - Gap, cy);
                g.DrawLine(pen, cx + Gap, cy, cx + Gap + Length, cy);
                break;

            case "x":
                float d0 = Gap * 0.7f, d1 = (Gap + Length) * 0.7f;
                g.DrawLine(pen, cx - d1, cy - d1, cx - d0, cy - d0);
                g.DrawLine(pen, cx + d0, cy - d0, cx + d1, cy - d1);
                g.DrawLine(pen, cx - d1, cy + d1, cx - d0, cy + d0);
                g.DrawLine(pen, cx + d0, cy + d0, cx + d1, cy + d1);
                if (Dot) DrawDot(g, pen, cx, cy, t);
                break;

            default: // cross
                if (ArmLeft) g.DrawLine(pen, cx - Gap - Length, cy, cx - Gap, cy);
                if (ArmRight) g.DrawLine(pen, cx + Gap, cy, cx + Gap + Length, cy);
                if (ArmTop) g.DrawLine(pen, cx, cy - Gap - Length, cx, cy - Gap);
                if (ArmBottom) g.DrawLine(pen, cx, cy + Gap, cx, cy + Gap + Length);
                if (Diagonal)
                {
                    float e0 = Gap * 0.7f, e1 = (Gap + Length) * 0.7f;
                    g.DrawLine(pen, cx - e1, cy - e1, cx - e0, cy - e0);
                    g.DrawLine(pen, cx + e0, cy - e0, cx + e1, cy - e1);
                    g.DrawLine(pen, cx - e1, cy + e1, cx - e0, cy + e0);
                    g.DrawLine(pen, cx + e0, cy + e0, cx + e1, cy + e1);
                }
                if (Dot) DrawDot(g, pen, cx, cy, t);
                break;
        }
    }

    private static void DrawDot(Graphics g, Pen pen, int cx, int cy, int t)
    {
        using var b = new SolidBrush(pen.Color);
        g.FillRectangle(b, cx - t / 2f, cy - t / 2f, t, t);
    }

    // the layered bitmap owns the pixels; nothing else should paint
    protected override void OnPaintBackground(PaintEventArgs e) { }
    protected override void OnPaint(PaintEventArgs e) { }
}