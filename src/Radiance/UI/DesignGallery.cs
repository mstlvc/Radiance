using System.Drawing.Drawing2D;
using Radiance.Core;

namespace Radiance.UI;

/// <summary>One gallery cell: live preview, name, and a favourite star.</summary>
public sealed class DesignTile : ThemedControl
{
    private bool _hover;
    private bool _selected;
    private bool _favourite;

    public CrosshairDesign? Design { get; set; }

    public event EventHandler? FavouriteToggled;

    public bool Selected
    {
        get => _selected;
        set { if (_selected == value) return; _selected = value; Invalidate(); }
    }

    public bool Favourite
    {
        get => _favourite;
        set { if (_favourite == value) return; _favourite = value; Invalidate(); }
    }

    public DesignTile()
    {
        Height = 72;
        Cursor = Cursors.Hand;
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnClick(EventArgs e)
    {
        // the star sits in the top-right corner
        var star = new Rectangle(Width - 26, 4, 22, 22);
        if (star.Contains(PointToClient(Cursor.Position)))
            FavouriteToggled?.Invoke(this, EventArgs.Empty);
        else
            base.OnClick(e);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var r = new Rectangle(0, 0, Width - 1, Height - 1);
        Draw.FillRounded(g, r, 10, _selected ? P.SurfaceAlt : (_hover ? P.Hover : P.Surface));
        Draw.StrokeRounded(g, r, 10, _selected ? P.Accent : P.Border, _selected ? 2f : 1f);

        if (Design is null) return;
        if (Width < 60 || Height < 40) return;

        DrawPreview(g, Design.Settings, new Point(26, Height / 2));

        if (Width > 90)
            Draw.Text(g, Design.Name, Theme.Small, P.Text, new Rectangle(50, 8, Width - 78, 18));

        // star
        if (Width > 40)
        {
            var starRect = new Rectangle(Width - 26, 4, 22, 22);
            using var starFont = new Font("Segoe UI", 11f);
            Draw.Text(g, _favourite ? "★" : "☆", starFont,
                _favourite ? P.Accent : P.TextMuted, starRect,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        }
    }

    /// <summary>Draws the same geometry the overlay uses, so the tile tells the truth.</summary>
    private static void DrawPreview(Graphics g, CrosshairSettings d, Point centre)
    {
        int cx = centre.X, cy = centre.Y;
        int t = Math.Max(1, d.Thickness);
        int len = Math.Max(2, d.Length);
        int gap = d.Gap;

        var ink = Color.FromArgb(d.ColourArgb);

        if (d.Outline) DrawShape(g, new Pen(Color.Black, t + Math.Max(1, d.OutlineThickness) * 2f), d, cx, cy, t, len, gap);
        DrawShape(g, new Pen(ink, t) { StartCap = LineCap.Flat, EndCap = LineCap.Flat }, d, cx, cy, t, len, gap);
    }

    private static void DrawShape(Graphics g, Pen pen, CrosshairSettings d, int cx, int cy, int t, int len, int gap)
    {
        using (pen)
        {
            switch (d.Shape)
            {
                case "dot":
                    using (var b = new SolidBrush(pen.Color))
                        g.FillEllipse(b, cx - t, cy - t, t * 2, t * 2);
                    break;

                case "circle":
                    int r = gap + len;
                    g.DrawEllipse(pen, cx - r, cy - r, r * 2, r * 2);
                    if (d.Dot) Dot(g, pen, cx, cy, t);
                    break;

                case "t":
                    Dot(g, pen, cx, cy, t);
                    g.DrawLine(pen, cx, cy + gap, cx, cy + gap + len);
                    g.DrawLine(pen, cx - gap - len, cy, cx - gap, cy);
                    g.DrawLine(pen, cx + gap, cy, cx + gap + len, cy);
                    break;

                case "x":
                    float d0 = gap * 0.7f, d1 = (gap + len) * 0.7f;
                    g.DrawLine(pen, cx - d1, cy - d1, cx - d0, cy - d0);
                    g.DrawLine(pen, cx + d0, cy - d0, cx + d1, cy - d1);
                    g.DrawLine(pen, cx - d1, cy + d1, cx - d0, cy + d0);
                    g.DrawLine(pen, cx + d0, cy + d0, cx + d1, cy + d1);
                    if (d.Dot) Dot(g, pen, cx, cy, t);
                    break;

                default:
                    if (d.ArmLeft) g.DrawLine(pen, cx - gap - len, cy, cx - gap, cy);
                    if (d.ArmRight) g.DrawLine(pen, cx + gap, cy, cx + gap + len, cy);
                    if (d.ArmTop) g.DrawLine(pen, cx, cy - gap - len, cx, cy - gap);
                    if (d.ArmBottom) g.DrawLine(pen, cx, cy + gap, cx, cy + gap + len);
                    if (d.Diagonal)
                    {
                        float e0 = gap * 0.7f, e1 = (gap + len) * 0.7f;
                        g.DrawLine(pen, cx - e1, cy - e1, cx - e0, cy - e0);
                        g.DrawLine(pen, cx + e0, cy - e0, cx + e1, cy - e1);
                        g.DrawLine(pen, cx - e1, cy + e1, cx - e0, cy + e0);
                        g.DrawLine(pen, cx + e0, cy + e0, cx + e1, cy + e1);
                    }
                    if (d.Dot) Dot(g, pen, cx, cy, t);
                    break;
            }
        }
    }

    private static void Dot(Graphics g, Pen pen, int cx, int cy, int t)
    {
        using var b = new SolidBrush(pen.Color);
        g.FillRectangle(b, cx - t / 2f, cy - t / 2f, t, t);
    }
}

/// <summary>
/// Gallery grid: four columns per row, filtered by family, favourites first —
/// the same arrangement the original app used.
/// </summary>
public sealed class DesignGallery : ThemedControl
{
    private const int Gap = 8;
    private const int TileHeight = 72;
    private const int Columns = 4;

    private readonly List<DesignTile> _tiles = new();
    private string _family = "Cross";
    private int _scroll;

    public event EventHandler<CrosshairDesign>? DesignChosen;
    public event EventHandler<CrosshairDesign>? FavouriteToggled;

    public DesignGallery()
    {
        Height = 232;
        SetStyle(ControlStyles.Selectable, true);
    }

    protected override bool HandlesOwnWheel => true;

    public void SetFamily(string family)
    {
        _family = family;
        Arrange();
    }

    public void Rebuild(IEnumerable<string>? favourites = null)
    {
        foreach (var t in _tiles) t.Dispose();
        _tiles.Clear();
        Controls.Clear();

        var fav = new HashSet<string>(favourites ?? Array.Empty<string>(), StringComparer.Ordinal);

        // favourites first, then the rest — matching the original ordering
        var ordered = CrosshairDesigns.All
            .OrderByDescending(d => fav.Contains(d.Id))
            .ToList();

        foreach (var d in ordered)
        {
            var tile = new DesignTile { Design = d, Favourite = fav.Contains(d.Id) };
            tile.Click += (_, _) => DesignChosen?.Invoke(this, d);
            tile.FavouriteToggled += (_, _) => FavouriteToggled?.Invoke(this, d);
            _tiles.Add(tile);
            Controls.Add(tile);
        }

        Arrange();
    }

    public void MarkSelected(CrosshairSettings settings)
    {
        var match = CrosshairDesigns.Match(settings);
        foreach (var t in _tiles)
            t.Selected = match is not null && t.Design?.Id == match.Id;
    }

    private void Arrange()
    {
        int colWidth = Math.Max(60, (Width - Gap * (Columns - 1)) / Columns);
        int row = 0;

        foreach (var tile in _tiles)
        {
            bool visible = tile.Design is not null &&
                           (string.IsNullOrEmpty(_family) || tile.Design.Family == _family);
            tile.Visible = visible;
            if (!visible) continue;

            int col = row % Columns;
            int r = row / Columns;
            tile.SetBounds(col * (colWidth + Gap), r * (TileHeight + Gap) - _scroll,
                colWidth, TileHeight);
            row++;
        }

        int rows = (row + Columns - 1) / Columns;
        int content = rows * (TileHeight + Gap);
        _scroll = Math.Clamp(_scroll, 0, Math.Max(0, content - Height));
    }

    protected override void OnResize(EventArgs e) { base.OnResize(e); Arrange(); }

    protected override void OnMouseWheel(MouseEventArgs e)
    {
        _scroll -= e.Delta / 3;
        Arrange();
        base.OnMouseWheel(e);
    }
}