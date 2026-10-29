using System.Drawing.Drawing2D;
using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class CrosshairPage : Page
{
    private readonly ShellForm _shell;
    private readonly CrosshairOverlay _overlay = new();

    private readonly ToggleRow _enabled = new() { Text = "Show crosshair" };
    private readonly SegmentedRow _family = new();
    private readonly DesignGallery _gallery = new();

    private readonly SegmentedRow _shape = new();
    private readonly SliderRow _length = new() { Label = "Arm length", Min = 2, Max = 40, Suffix = " px" };
    private readonly SliderRow _thickness = new() { Label = "Thickness", Min = 1, Max = 8, Suffix = " px" };
    private readonly SliderRow _gap = new() { Label = "Centre gap", Min = 0, Max = 30, Suffix = " px" };
    private readonly SliderRow _opacity = new() { Label = "Opacity", Min = 20, Max = 100 };

    private readonly ColourWheel _wheel = new() { Height = 180 };
    private readonly TextBox _hex = new();
    private readonly Panel _swatch = new() { Width = 32, Height = 26, Cursor = Cursors.Default };

    private readonly ToggleRow _dot = new() { Text = "Centre dot" };
    private readonly ToggleRow _outline = new() { Text = "Black outline" };
    private readonly SliderRow _outlineThickness = new() { Label = "Outline thickness", Min = 1, Max = 4, Suffix = " px" };
    private readonly ToggleRow _diagonal = new() { Text = "Diagonal arms" };
    private readonly ToggleRow _arms = new() { Text = "Show all four arms" };

    private readonly TextBox _share = new();
    private readonly Caption _status = new();

    private bool _loading = true;

    public override string Title => "Crosshair";
    public override string Subtitle => "A click-through overlay with a design gallery.";

    public CrosshairPage(ShellForm shell)
    {
        _shell = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        var overlay = stack.AddManual("Overlay", 470, "The overlay never takes focus or clicks.");
        overlay.AddAll(_enabled, _family);

        _gallery.Left = 0;
        _gallery.Top = 96;
        _gallery.Width = Math.Max(200, overlay.Width);
        overlay.Add(_gallery);
        overlay.Resize += (_, _) => { _gallery.Width = Math.Max(200, overlay.ClientSize.Width - 40); _gallery.Invalidate(); };

        var shape = stack.Add("Shape");
        shape.AddAll(_shape, _arms, _diagonal, _length, _thickness, _gap, _opacity);

        var look = stack.Add("Colour", "Drag the wheel, or type a hex value.");
        BuildColour(look);
        look.AddAll(_dot, _outline, _outlineThickness);

        var share = stack.Add("Share", "Copy this crosshair as a code, or paste one in.");
        BuildShare(share);

        stack.AddRow(24).Controls.Add(_status);

        _family.SetItems(CrosshairDesigns.Families.Select(f => (f, f)));
        _gallery.SetFamily("Cross");
        _gallery.Rebuild(_shell.Settings.FavouriteCrosshairs);

        _family.SelectionChanged += (_, _) => _gallery.SetFamily(_family.Selected);

        _gallery.DesignChosen += (_, d) =>
        {
            CrosshairDesigns.Apply(_shell.Settings.Crosshair, d);
            LoadState();
            _shell.SaveSettings();
            _status.Text = $"Applied “{d.Name}”.";
            _status.Invalidate();
        };

        _gallery.FavouriteToggled += (_, d) =>
        {
            var favs = _shell.Settings.FavouriteCrosshairs;
            if (!favs.Remove(d.Id)) favs.Add(d.Id);
            _gallery.Rebuild(favs);
            _gallery.SetFamily(_family.Selected);
            _gallery.MarkSelected(_shell.Settings.Crosshair);
            _shell.SaveSettings();
        };

        _shape.SetItems(new[]
        {
            ("cross", "Cross"), ("dot", "Dot"), ("circle", "Circle"), ("t", "T"), ("x", "X")
        });

        _shape.SelectionChanged += (_, _) => { UpdateShapeFields(); Sync(); };
        _enabled.CheckedChanged += (_, _) => Sync();
        _dot.CheckedChanged += (_, _) => Sync();
        _outline.CheckedChanged += (_, _) =>
        {
            _outlineThickness.Enabled = _outline.Checked;
            _outlineThickness.Invalidate();
            Sync();
        };
        _diagonal.CheckedChanged += (_, _) => Sync();
        _arms.CheckedChanged += (_, _) => Sync();
        _outlineThickness.ValueChanged += (_, _) => Sync();

        foreach (var s in new Control[] { _length, _thickness, _gap, _opacity })
            ((SliderRow)s).ValueChanged += (_, _) => Sync();

        _wheel.ColourChanged += (_, _) =>
        {
            if (_loading) return;
            _hex.Text = _wheel.Hex;
            _swatch.Invalidate();
            Sync();
        };

        _hex.Leave += (_, _) => ApplyHex();
        _hex.KeyDown += (_, e) => { if (e.KeyCode == Keys.Enter) { ApplyHex(); e.SuppressKeyPress = true; } };

        Theme.Changed += () =>
        {
            _hex.BackColor = Theme.Palette.SurfaceAlt;
            _hex.ForeColor = Theme.Palette.Text;
            _swatch.Invalidate();
        };

        LoadState();
        UpdateShapeFields();
    }

    private void BuildColour(Card host)
    {
        host.Add(_wheel);

        var hexRow = new Panel { Height = 34, BackColor = Color.Transparent };

        var lbl = new Caption { Text = "Hex code", Left = 0, Top = 8, Width = 68, Height = 20 };

        _hex.Left = 72;
        _hex.Top = 4;
        _hex.Width = 90;
        _hex.Height = 26;
        _hex.Font = Theme.Body;
        _hex.MaxLength = 7;
        _hex.TextAlign = HorizontalAlignment.Center;
        _hex.BackColor = Theme.Palette.SurfaceAlt;
        _hex.ForeColor = Theme.Palette.Text;
        _hex.BorderStyle = BorderStyle.FixedSingle;

        _swatch.Left = 170;
        _swatch.Top = 4;
        _swatch.Width = 32;
        _swatch.Height = 26;
        _swatch.Paint += (_, e) =>
        {
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            var r = new Rectangle(0, 0, _swatch.Width - 1, _swatch.Height - 1);
            Draw.FillRounded(g, r, 6, _wheel.Colour);
            Draw.StrokeRounded(g, r, 6, Theme.Palette.Border, 1.5f);
        };

        hexRow.Controls.Add(lbl);
        hexRow.Controls.Add(_hex);
        hexRow.Controls.Add(_swatch);

        // quick preset color dots
        int px = 216;
        var presets = new (string name, Color color)[]
        {
            ("Green", Color.FromArgb(0x00, 0xFF, 0x00)),
            ("Cyan", Color.FromArgb(0x00, 0xE5, 0xFF)),
            ("Yellow", Color.FromArgb(0xFF, 0xEA, 0x00)),
            ("Red", Color.FromArgb(0xFF, 0x2A, 0x6D)),
            ("Magenta", Color.FromArgb(0xD9, 0x46, 0xEF)),
            ("White", Color.White)
        };

        foreach (var (name, col) in presets)
        {
            var dot = new Panel
            {
                Left = px,
                Top = 7,
                Width = 20,
                Height = 20,
                Cursor = Cursors.Hand
            };
            dot.Paint += (_, e) =>
            {
                var g = e.Graphics;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                var r = new Rectangle(0, 0, dot.Width - 1, dot.Height - 1);
                Draw.FillRounded(g, r, 10, col);
                Draw.StrokeRounded(g, r, 10, Theme.Palette.Border);
            };
            dot.Click += (_, _) =>
            {
                _wheel.SetColour(col, raise: true);
                _hex.Text = _wheel.Hex;
                _swatch.Invalidate();
            };
            hexRow.Controls.Add(dot);
            px += 26;
        }

        host.Add(hexRow);
    }

    private void BuildShare(Card host)
    {
        _share.Left = 0;
        _share.Top = 4;
        _share.Width = 340;
        _share.Height = 28;
        _share.Font = Theme.Body;
        _share.BorderStyle = BorderStyle.FixedSingle;
        _share.PlaceholderText = "rdcx-…";
        host.Add(_share);

        var row = new Panel { Height = 36, BackColor = Color.Transparent };
        var copy = new FlatButton { Text = "Copy", Accent = false, Width = 80, Height = 30, Top = 3 };
        var paste = new FlatButton { Text = "Apply code", Width = 110, Height = 30, Left = 90, Top = 3 };
        copy.Click += (_, _) => CopyCode();
        paste.Click += (_, _) => ApplyCode();
        row.Controls.Add(copy);
        row.Controls.Add(paste);
        host.Add(row);
    }

    private void CopyCode()
    {
        _share.Text = ShareCodes.EncodeCrosshair(_shell.Settings.Crosshair);
        try { Clipboard.SetText(_share.Text); _status.Text = "Crosshair code copied."; } catch { }
        _status.Invalidate();
    }

    private void ApplyCode()
    {
        if (!ShareCodes.TryDecodeCrosshair(_share.Text.Trim(), out var cfg))
        {
            _status.Text = "That does not look like a crosshair code.";
            _status.Invalidate();
            return;
        }

        var target = _shell.Settings.Crosshair;
        target.Shape = cfg.Shape;
        target.Length = cfg.Length;
        target.Thickness = cfg.Thickness;
        target.Gap = cfg.Gap;
        target.Opacity = cfg.Opacity;
        target.Dot = cfg.Dot;
        target.Outline = cfg.Outline;
        target.ColourArgb = cfg.ColourArgb;

        LoadState();
        _shell.SaveSettings();
        _status.Text = "Crosshair code applied.";
        _status.Invalidate();
    }

    private void ApplyHex()
    {
        if (ColourWheel.TryParseHex(_hex.Text, out var c))
        {
            _loading = true;
            _wheel.SetColour(c);
            _hex.Text = _wheel.Hex;
            _swatch.Invalidate();
            _loading = false;
            Sync();
        }
        else
        {
            _hex.Text = _wheel.Hex;
            _swatch.Invalidate();
        }
    }

    private void LoadState()
    {
        var c = _shell.Settings.Crosshair;
        _loading = true;
        _enabled.Checked = c.Enabled;
        _shape.Select(c.Shape);
        _length.Value = c.Length;
        _thickness.Value = c.Thickness;
        _gap.Value = c.Gap;
        _opacity.Value = c.Opacity;
        _dot.Checked = c.Dot;
        _outline.Checked = c.Outline;
        _outlineThickness.Value = c.OutlineThickness;
        _outlineThickness.Enabled = c.Outline;
        _diagonal.Checked = c.Diagonal;
        _arms.Checked = c.ArmTop && c.ArmBottom && c.ArmLeft && c.ArmRight;
        _wheel.SetColour(Color.FromArgb(c.ColourArgb));
        _hex.Text = _wheel.Hex;
        _swatch.Invalidate();
        _loading = false;

        _gallery.MarkSelected(c);
        UpdateShapeFields();
        Sync();
    }

    private void UpdateShapeFields()
    {
        bool isDot = _shape.Selected == "dot";
        _length.Enabled = !isDot;
        _gap.Enabled = !isDot;
        _length.Invalidate();
        _gap.Invalidate();
    }

    private void Sync()
    {
        if (_loading) return;

        var c = _shell.Settings.Crosshair;

        c.Enabled = _enabled.Checked;
        c.Shape = _shape.Selected;
        c.Length = _length.Value;
        c.Thickness = _thickness.Value;
        c.Gap = _gap.Value;
        c.Opacity = _opacity.Value;
        c.Dot = _dot.Checked;
        c.Outline = _outline.Checked;
        c.OutlineThickness = _outlineThickness.Value;
        c.Diagonal = _diagonal.Checked;
        c.ArmTop = c.ArmBottom = c.ArmLeft = c.ArmRight = _arms.Checked;
        c.ColourArgb = _wheel.Colour.ToArgb();

        _overlay.Shape = c.Shape;
        _overlay.Ink = _wheel.Colour;
        _overlay.Length = c.Length;
        _overlay.Thickness = c.Thickness;
        _overlay.Gap = c.Gap;
        _overlay.Dot = c.Dot;
        _overlay.Outline = c.Outline;
        _overlay.OutlineThickness = c.OutlineThickness;
        _overlay.Diagonal = c.Diagonal;
        _overlay.ArmTop = c.ArmTop;
        _overlay.ArmBottom = c.ArmBottom;
        _overlay.ArmLeft = c.ArmLeft;
        _overlay.ArmRight = c.ArmRight;
        _overlay.InkOpacity = c.Opacity / 100.0;

        if (c.Enabled)
        {
            if (!_overlay.Visible) _overlay.Show();
            _overlay.BringToFront();

            // the layered window is repainted from a bitmap, not through Invalidate
            _overlay.Redraw();
        }
        else if (_overlay.Visible)
        {
            _overlay.Hide();
        }

        _swatch.Invalidate();
        _shell.SaveSettings();
    }

    public override void OnShown() => Sync();

    /// <summary>Global-hotkey entry point.</summary>
    public void ToggleFromHotkey() => _enabled.Checked = !_enabled.Checked;

    protected override void Dispose(bool disposing)
    {
        if (disposing) _overlay.Dispose();
        base.Dispose(disposing);
    }
}
