using System.Drawing.Drawing2D;

namespace Radiance.UI;

/// <summary>Base for every custom-painted control: double buffered and theme-aware.</summary>
public abstract class ThemedControl : Control
{
    protected ThemedControl()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint
               | ControlStyles.OptimizedDoubleBuffer
               | ControlStyles.ResizeRedraw
               | ControlStyles.UserPaint
               | ControlStyles.SupportsTransparentBackColor, true);
        BackColor = Color.Transparent;
        Theme.Changed += OnThemeChanged;
    }

    private void OnThemeChanged() => Invalidate();

    protected override void Dispose(bool disposing)
    {
        if (disposing) Theme.Changed -= OnThemeChanged;
        base.Dispose(disposing);
    }

    protected Palette P => Theme.Palette;

    /// <summary>True for controls that scroll themselves and must not move the page.</summary>
    protected virtual bool HandlesOwnWheel => false;

    protected override void OnMouseWheel(MouseEventArgs e)
    {
        if (!HandlesOwnWheel)
        {
            for (Control? parent = Parent; parent is not null; parent = parent.Parent)
            {
                if (parent is CardStack stack)
                {
                    stack.ScrollBy(e.Delta);
                    return;
                }
            }
        }

        base.OnMouseWheel(e);
    }
}

/// <summary>
/// Rounded card with an optional heading that lays its children out vertically.
/// Height is derived from content, so pages never hard-code it.
/// </summary>
public class Card : ThemedControl
{
    private const int PadX = 20;
    private const int PadY = 16;
    private const int Gap = 6;
    private const int HeaderHeight = 28;
    private const int NoteHeight = 20;

    private string _header = "";

    public string Header
    {
        get => _header;
        set { _header = value ?? ""; Relayout(); }
    }

    private string? _note;
    public string? Note
    {
        get => _note;
        set { _note = value; Relayout(); }
    }

    public int Radius { get; set; } = 12;

    /// <summary>When set, the card keeps this height instead of measuring its children.</summary>
    public int? ManualHeight { get; set; }

    public Card()
    {
        Height = PadY * 2;
    }

    public Card Add(Control child)
    {
        Controls.Add(child);
        Relayout();
        return this;
    }

    public void AddAll(params Control[] children)
    {
        foreach (var c in children) Controls.Add(c);
        Relayout();
    }

    public void Relayout()
    {
        if (IsDisposed) return;

        int top = PadY + (_header.Length > 0 ? HeaderHeight : 0) + (Note is { Length: > 0 } ? NoteHeight : 0);
        int width = Math.Max(0, ClientSize.Width - PadX * 2);

        foreach (Control c in Controls)
        {
            c.SetBounds(PadX, top, width, c.Height);
            top += c.Height + Gap;
        }

        int height = ManualHeight ?? (top - Gap + PadY);
        if (Height != height) Height = height;
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        Relayout();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var r = new Rectangle(0, 0, Width - 1, Height - 1);
        Draw.FillRounded(g, r, Radius, P.Surface);
        Draw.StrokeRounded(g, r, Radius, P.Border);

        int innerWidth = Math.Max(0, Width - PadX * 2);
        int textTop = PadY - 2;

        if (_header.Length > 0 && innerWidth > 0)
        {
            Draw.Text(g, _header, Theme.H2, P.Text, new Rectangle(PadX, textTop, innerWidth, 24));
            textTop += 24;
        }

        if (Note is { Length: > 0 } && innerWidth > 0)
        {
            Draw.Text(g, Note, Theme.Small, P.TextMuted,
                new Rectangle(PadX, textTop, innerWidth, 18));
        }
    }
}

/// <summary>Thin themed scrollbar; replaces the stock white Windows one.</summary>
public sealed class ThemedScrollBar : ThemedControl
{
    private int _value;
    private int _maximum;
    private int _viewport = 100;
    private bool _dragging;
    private bool _hover;
    private int _dragStartY;
    private int _dragStartValue;

    public event EventHandler? ValueChanged;

    public ThemedScrollBar()
    {
        Width = 10;
        Cursor = Cursors.Hand;
    }

    protected override bool HandlesOwnWheel => true;

    public int Value
    {
        get => _value;
        set
        {
            int v = Math.Clamp(value, 0, Math.Max(0, _maximum));
            if (v == _value) return;
            _value = v;
            Invalidate();
            ValueChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public int Maximum
    {
        get => _maximum;
        set { _maximum = Math.Max(0, value); Value = _value; Invalidate(); }
    }

    public int Viewport
    {
        get => _viewport;
        set { _viewport = Math.Max(1, value); Value = _value; Invalidate(); }
    }

    public bool Needed => _maximum > 0;
    private int MaxContent => _maximum + _viewport;

    private Rectangle TrackRect => new Rectangle(3, 2, Math.Max(2, Width - 6), Math.Max(8, Height - 4));

    private Rectangle ThumbRect()
    {
        var track = TrackRect;
        if (!Needed) return Rectangle.Empty;

        double ratio = (double)_viewport / MaxContent;
        int thumbH = Math.Max(28, (int)(track.Height * ratio));
        int travel = track.Height - thumbH;
        int offset = _maximum <= 0 ? 0 : (int)((double)_value / _maximum * travel);

        return new Rectangle(track.X, track.Y + offset, track.Width, thumbH);
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        Draw.FillRounded(g, TrackRect, 3, P.SurfaceAlt);

        if (!Needed) return;

        var thumb = ThumbRect();
        var colour = _dragging ? P.Accent : _hover ? P.TextMuted : Mix(P.TextMuted, P.Surface, 0.45f);
        Draw.FillRounded(g, thumb, 3, colour);
    }

    private static Color Mix(Color a, Color b, float t) => Color.FromArgb(
        (int)(a.R + (b.R - a.R) * t),
        (int)(a.G + (b.G - a.G) * t),
        (int)(a.B + (b.B - a.B) * t));

    protected override void OnMouseDown(MouseEventArgs e)
    {
        if (!Needed) return;

        var thumb = ThumbRect();
        if (thumb.Contains(e.Location))
        {
            _dragging = true;
            _dragStartY = e.Y;
            _dragStartValue = _value;
            Capture = true;
        }
        else
        {
            Value += e.Y < thumb.Y ? -_viewport : _viewport;
        }

        base.OnMouseDown(e);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_dragging)
        {
            var track = TrackRect;
            int thumbH = Math.Max(28, (int)(track.Height * (double)_viewport / MaxContent));
            int travel = Math.Max(1, track.Height - thumbH);
            int dy = e.Y - _dragStartY;
            Value = _dragStartValue + (int)((double)dy / travel * _maximum);
        }
        base.OnMouseMove(e);
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        _dragging = false;
        Capture = false;
        Invalidate();
        base.OnMouseUp(e);
    }

    protected override void OnMouseWheel(MouseEventArgs e)
    {
        Value -= Math.Sign(e.Delta) * Math.Max(40, _viewport / 6);
        base.OnMouseWheel(e);
    }
}

/// <summary>Vertical list of cards with scrolling and correct top-down order.</summary>
public sealed class CardStack : Panel
{
    private const int Gap = 16;

    private readonly ThemedScrollBar _bar = new();
    private readonly List<Control> _items = new();
    private readonly System.Windows.Forms.Timer _glide = new() { Interval = 15 };

    private int _offset;
    private int _target;
    private int _maximum;
    private bool _syncingBar;

    public CardStack()
    {
        Dock = DockStyle.Fill;
        BackColor = Color.Transparent;
        AutoScroll = false;

        _bar.Dock = DockStyle.Right;
        _bar.ValueChanged += (_, _) =>
        {
            // ignore our own updates, or the glide cancels itself every frame
            if (_syncingBar) return;

            // dragging the bar jumps straight there; the wheel glides separately
            _glide.Stop();
            _offset = _target = _bar.Value;
            Restack();
        };
        Controls.Add(_bar);

        // smooth scrolling: ease toward the target instead of jumping
        _glide.Tick += (_, _) =>
        {
            int diff = _target - _offset;
            if (Math.Abs(diff) <= 1)
            {
                _offset = _target;
                _glide.Stop();
            }
            else
            {
                _offset += (int)Math.Round(diff * 0.32);
            }
            Restack();
        };
    }

    public Card Add(string header, string? note = null)
    {
        var card = new Card { Header = header, Note = note };
        AddItem(card);
        card.SizeChanged += (_, _) => Restack();
        Restack();
        return card;
    }

    private void AddItem(Control c)
    {
        _items.Add(c);
        Controls.Add(c);
    }

    /// <summary>Adds a card whose content is laid out manually (galleries, grids).</summary>
    public Card AddManual(string header, int height, string? note = null)
    {
        var card = new Card { Header = header, Note = note };
        AddItem(card);
        card.ManualHeight = height;
        card.SizeChanged += (_, _) => Restack();
        Restack();
        return card;
    }

    /// <summary>Free row (buttons, links) that participates in the stack.</summary>
    public Panel AddRow(int height = 44)
    {
        var row = new Panel { Height = height, BackColor = Color.Transparent };
        AddItem(row);
        Restack();
        return row;
    }

    /// <summary>Scrolls by a wheel delta, easing to the new position.</summary>
    public void ScrollBy(int delta)
    {
        int step = Math.Max(44, Math.Max(1, ClientSize.Height) / 5);
        GlideTo(_target - Math.Sign(delta) * step);
    }

    /// <summary>Moves the page toward a position over a few frames.</summary>
    public void GlideTo(int position)
    {
        _target = Math.Clamp(position, 0, _maximum);
        if (_target == _offset) { _glide.Stop(); return; }
        _glide.Start();
    }

    private int AvailableWidth() => Math.Max(200, ClientSize.Width - 2 - _bar.Width - 4);

    private void Restack()
    {
        if (IsDisposed) return;

        int contentHeight = 0;
        foreach (var c in _items) contentHeight += c.Height + Gap;
        contentHeight = Math.Max(0, contentHeight - Gap);

        int viewport = Math.Max(1, ClientSize.Height);
        int maximum = Math.Max(0, contentHeight - viewport);
        _maximum = maximum;

        _bar.Maximum = maximum;
        _bar.Viewport = viewport;
        _bar.Visible = maximum > 0;

        _offset = Math.Clamp(_offset, 0, maximum);
        _target = Math.Clamp(_target, 0, maximum);
        if (_bar.Value != _offset)
        {
            _syncingBar = true;
            _bar.Value = _offset;
            _syncingBar = false;
        }

        int w = AvailableWidth();
        int y = -_offset;

        foreach (var c in _items)
        {
            c.SetBounds(0, y, w, c.Height);
            y += c.Height + Gap;
        }
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        Restack();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _glide.Stop(); _glide.Dispose(); }
        base.Dispose(disposing);
    }
}

/// <summary>Left-hand navigation row with an accent bar when selected.</summary>
public class NavButton : ThemedControl
{
    private bool _hover;
    private bool _selected;

    public bool Selected
    {
        get => _selected;
        set { if (_selected == value) return; _selected = value; Invalidate(); }
    }

    public string Glyph { get; set; } = "";

    public NavButton()
    {
        Height = 40;
        Cursor = Cursors.Hand;
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var r = new Rectangle(8, 2, Width - 16, Height - 4);
        if (_selected) Draw.FillRounded(g, r, 8, P.Surface);
        else if (_hover) Draw.FillRounded(g, r, 8, P.Hover);

        if (_selected) Draw.FillRounded(g, new Rectangle(0, 10, 3, Height - 20), 2, P.Accent);

        int x = 24;
        if (Glyph.Length > 0)
        {
            using var glyphFont = new Font("Segoe MDL2 Assets", 10f);
            Draw.Text(g, Glyph, glyphFont, _selected ? P.Accent : P.TextMuted,
                new Rectangle(x, 0, 22, Height), TextFormatFlags.VerticalCenter | TextFormatFlags.Left);
            x += 28;
        }

        var font = _selected ? new Font("Segoe UI", 9.75f, FontStyle.Bold) : Theme.Body;
        Draw.Text(g, Text, font, _selected ? P.Text : P.TextMuted,
            new Rectangle(x, 0, Width - x - 12, Height));
    }
}

/// <summary>Labelled slider with a live value badge.</summary>
public class SliderRow : ThemedControl
{
    private bool _dragging;

    public int Min { get; set; } = 0;
    public int Max { get; set; } = 100;
    public int Step { get; set; } = 1;
    public string Suffix { get; set; } = "";
    public string Label { get; set; } = "Value";

    private int _value = 50;
    public int Value
    {
        get => _value;
        set
        {
            int v = Math.Clamp(value, Min, Max);
            if (v == _value) return;
            _value = v;
            Invalidate();
            ValueChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public event EventHandler? ValueChanged;

    public SliderRow()
    {
        Height = 54;
        TabStop = true;
        Cursor = Cursors.Hand;
    }

    private Rectangle Track => new(0, Height - 16, Math.Max(1, Width), 6);

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        bool on = Enabled;
        Draw.Text(g, Label, Theme.Body, on ? P.Text : P.TextMuted, new Rectangle(0, 0, Width - 90, 22));

        string text = $"{Value}{Suffix}";
        var badge = new Rectangle(Width - 62, 2, 62, 20);
        Draw.FillRounded(g, badge, 10, P.SurfaceAlt);
        Draw.Text(g, text, Theme.Small, on ? P.Text : P.TextMuted, badge,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);

        var track = Track;
        Draw.FillRounded(g, track, 3, P.Track);

        int knobX = KnobX();
        var accent = on ? P.Accent : P.TextMuted;
        Draw.FillRounded(g, new Rectangle(track.X, track.Y, Math.Max(2, knobX - track.X), track.Height), 3, accent);

        var k = new Rectangle(knobX - 6, track.Y + track.Height / 2 - 6, 12, 12);
        using var fill = new SolidBrush(P.Surface);
        g.FillEllipse(fill, k);
        using var ring = new Pen(accent, 2f);
        g.DrawEllipse(ring, k);
    }

    private int KnobX()
    {
        int usable = Math.Max(1, Width - 13);
        double t = (double)(Value - Min) / Math.Max(1, Max - Min);
        return (int)(6 + t * usable);
    }

    private void SetFromX(int x)
    {
        int usable = Math.Max(1, Width - 13);
        double t = Math.Clamp((x - 6.0) / usable, 0, 1);
        int raw = (int)Math.Round(Min + t * (Max - Min));
        if (Step > 1) raw = (int)(Math.Round((double)raw / Step) * Step);
        Value = raw;
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        if (Enabled && e.Button == MouseButtons.Left) { _dragging = true; Focus(); SetFromX(e.X); }
        base.OnMouseDown(e);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_dragging) SetFromX(e.X);
        base.OnMouseMove(e);
    }

    protected override void OnMouseUp(MouseEventArgs e) { _dragging = false; base.OnMouseUp(e); }

    protected override bool IsInputKey(Keys keyData) =>
        keyData is Keys.Left or Keys.Right or Keys.Home or Keys.End || base.IsInputKey(keyData);

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode is Keys.Left or Keys.Down) { Value -= Step; e.Handled = true; }
        if (e.KeyCode is Keys.Right or Keys.Up) { Value += Step; e.Handled = true; }
        if (e.KeyCode == Keys.Home) { Value = Min; e.Handled = true; }
        if (e.KeyCode == Keys.End) { Value = Max; e.Handled = true; }
        base.OnKeyDown(e);
    }
}

/// <summary>Label plus pill switch.</summary>
public class ToggleRow : ThemedControl
{
    private bool _checked;

    public bool Checked
    {
        get => _checked;
        set
        {
            if (_checked == value) return;
            _checked = value;
            Invalidate();
            CheckedChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public event EventHandler? CheckedChanged;

    public ToggleRow()
    {
        Height = 42;
        Cursor = Cursors.Hand;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        Draw.Text(g, Text, Theme.Body, P.Text, new Rectangle(0, 0, Width - 56, Height));

        var pill = new Rectangle(Width - 44, Height / 2 - 11, 44, 22);
        Draw.FillRounded(g, pill, 11, _checked ? P.Accent : P.Track);

        int knobX = _checked ? pill.Right - 19 : pill.X + 2;
        using var knob = new SolidBrush(_checked ? P.AccentText : P.TextMuted);
        g.FillEllipse(knob, knobX, pill.Y + 2, 18, 18);
    }

    protected override void OnClick(EventArgs e) { Checked = !Checked; base.OnClick(e); }
}

/// <summary>Rounded button.</summary>
public class FlatButton : ThemedControl
{
    private bool _hover;

    public bool Accent { get; set; } = true;
    public bool Selected { get; set; }

    public FlatButton()
    {
        Height = 34;
        Cursor = Cursors.Hand;
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; Invalidate(); base.OnMouseLeave(e); }

    public void SetSelected(bool value) { Selected = value; Invalidate(); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var r = new Rectangle(0, 0, Width - 1, Height - 1);

        Color bg, fg;
        if (Selected) { bg = P.Accent; fg = P.AccentText; }
        else if (Accent) { bg = _hover ? ControlPaint.Light(P.Accent, 0.14f) : P.Accent; fg = P.AccentText; }
        else { bg = _hover ? P.Hover : P.SurfaceAlt; fg = P.Text; }

        Draw.FillRounded(g, r, 8, bg);
        if (!Accent && !Selected) Draw.StrokeRounded(g, r, 8, P.Border);

        Draw.Text(g, Text, Theme.Body, fg, r,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
    }
}

/// <summary>Segmented selector: a row of mutually exclusive chips.</summary>
public class SegmentedRow : ThemedControl
{
    private readonly List<(string Key, string Label)> _items = new();
    private readonly Dictionary<string, Rectangle> _hit = new();
    private string _selected = "";

    public event EventHandler? SelectionChanged;
    public string Selected => _selected;

    public SegmentedRow()
    {
        Height = 38;
        Cursor = Cursors.Hand;
    }

    public void SetItems(IEnumerable<(string Key, string Label)> items, string? select = null)
    {
        _items.Clear();
        _items.AddRange(items);
        _selected = select ?? (_items.Count > 0 ? _items[0].Key : "");
        Invalidate();
    }

    public void Select(string key)
    {
        if (_selected == key) return;
        _selected = key;
        Invalidate();
        SelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        _hit.Clear();

        if (_items.Count == 0) return;

        int gap = 6;
        int w = Math.Max(40, (Width - gap * (_items.Count - 1)) / _items.Count);
        int x = 0;

        foreach (var (key, label) in _items)
        {
            var r = new Rectangle(x, 2, w, Height - 4);
            _hit[key] = r;

            bool on = key == _selected;
            Draw.FillRounded(g, r, 8, on ? P.Accent : P.SurfaceAlt);
            if (!on) Draw.StrokeRounded(g, r, 8, P.Border);

            Draw.Text(g, label, Theme.Small, on ? P.AccentText : P.TextMuted, r,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            x += w + gap;
        }
    }

    protected override void OnClick(EventArgs e)
    {
        foreach (var kv in _hit)
        {
            if (kv.Value.Contains(PointToClient(Cursor.Position)))
            {
                Select(kv.Key);
                break;
            }
        }
        base.OnClick(e);
    }
}

/// <summary>Row of colour swatches.</summary>
public class SwatchRow : ThemedControl
{
    private static readonly Color[] Palette_ =
    {
        Color.Lime, Color.Red, Color.Cyan, Color.Yellow,
        Color.Magenta, Color.White, Color.Orange, Color.HotPink
    };

    private Color _selected = Color.Lime;
    public event EventHandler? SelectionChanged;
    public Color Selected => _selected;

    public SwatchRow()
    {
        Height = 36;
        Cursor = Cursors.Hand;
    }

    public void Select(Color c) { _selected = c; Invalidate(); SelectionChanged?.Invoke(this, EventArgs.Empty); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        int x = 0;
        foreach (var c in Palette_)
        {
            var r = new Rectangle(x + 2, 3, 28, 28);
            Draw.FillRounded(g, r, 8, c);
            if (c.ToArgb() == _selected.ToArgb())
                Draw.StrokeRounded(g, new Rectangle(r.X - 2, r.Y - 2, r.Width + 4, r.Height + 4), 10, P.Text, 2f);
            x += 34;
        }
    }

    protected override void OnClick(EventArgs e)
    {
        // hit test in control coordinates
        var local = PointToClient(Cursor.Position);
        int i = local.X / 34;
        if (i >= 0 && i < Palette_.Length) Select(Palette_[i]);
        base.OnClick(e);
    }
}

/// <summary>Small muted caption.</summary>
public class Caption : ThemedControl
{
    /// <summary>Optional colour override; null uses the muted text colour.</summary>
    public Color? Ink { get; set; }

    public Caption() { Height = 20; }

    protected override void OnPaint(PaintEventArgs e) =>
        Draw.Text(e.Graphics, Text, Theme.Small, Ink ?? P.TextMuted, ClientRectangle);
}

/// <summary>Selectable list row used for displays, games and profiles.</summary>
public class ListRow : ThemedControl
{
    private bool _selected;
    private bool _hover;

    public bool Selected
    {
        get => _selected;
        set { if (_selected == value) return; _selected = value; Invalidate(); }
    }

    public string Detail { get; set; } = "";
    public event EventHandler? Activated;

    public ListRow()
    {
        Height = 48;
        Cursor = Cursors.Hand;
    }

    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var r = new Rectangle(0, 0, Width - 1, Height - 1);
        Draw.FillRounded(g, r, 8, _selected ? P.SurfaceAlt : (_hover ? P.Hover : P.Surface));

        if (_selected)
        {
            Draw.FillRounded(g, new Rectangle(0, 10, 3, Height - 20), 2, P.Accent);
        }

        Draw.Text(g, Text, Theme.Body, _selected ? P.Text : P.Text, new Rectangle(14, 6, Width - 28, 20));

        if (Detail.Length > 0)
            Draw.Text(g, Detail, Theme.Small, P.TextMuted, new Rectangle(14, 24, Width - 28, 18));
    }

    protected override void OnDoubleClick(EventArgs e) { Activated?.Invoke(this, EventArgs.Empty); base.OnDoubleClick(e); }
}

public static class Draw
{
    public static GraphicsPath Rounded(Rectangle r, int radius)
    {
        var p = new GraphicsPath();
        int d = Math.Max(0, radius * 2);
        if (d == 0) { p.AddRectangle(r); return p; }
        d = Math.Min(d, Math.Min(r.Width, r.Height));
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
        if (r.Width <= 1 || r.Height <= 1) return;
        var inner = new Rectangle(r.X + (int)width, r.Y + (int)width,
            Math.Max(1, r.Width - (int)width * 2), Math.Max(1, r.Height - (int)width * 2));
        using var path = Rounded(inner, radius);
        using var pen = new Pen(color, width);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.DrawPath(pen, path);
    }

    public static void Text(Graphics g, string text, Font font, Color color, Rectangle bounds,
        TextFormatFlags flags = TextFormatFlags.Left | TextFormatFlags.VerticalCenter)
    {
        if (string.IsNullOrEmpty(text)) return;

        // TextRenderer throws "Parameter is not valid" for empty bounds; a tile
        // narrower than its text does exactly that while it is being laid out.
        if (bounds.Width <= 0 || bounds.Height <= 0) return;

        bounds.Width = Math.Min(bounds.Width, 10000);
        bounds.Height = Math.Min(bounds.Height, 10000);

        TextRenderer.DrawText(g, text, font, bounds, color,
            flags | TextFormatFlags.NoPrefix | TextFormatFlags.EndEllipsis);
    }
}
