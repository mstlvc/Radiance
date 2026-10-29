using System.Drawing.Drawing2D;
using Radiance.Core;

namespace Radiance.UI;

/// <summary>First-run wizard: welcome, theme, live vibrance, done.</summary>
public sealed class OnboardingForm : Form
{
    private readonly ColorPipeline _pipeline;
    private readonly FlatButton _next = new() { Text = "Get started", Width = 140, Height = 36 };
    private readonly FlatButton _back = new() { Text = "Back", Accent = false, Width = 90, Height = 36 };
    private readonly ToggleRow _light = new() { Text = "Use the light theme" };
    private readonly SliderRow _vibrance = new() { Label = "Try it now — vibrance", Min = 0, Max = 100 };

    private int _step;
    private int _originalVibrance = 50;

    public OnboardingForm(ColorPipeline pipeline)
    {
        _pipeline = pipeline;
        _originalVibrance = 50;

        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.CenterParent;
        Size = new Size(580, 400);
        BackColor = Theme.Palette.Surface;
        DoubleBuffered = true;

        Build();
        Repaint();
    }

    private void Build()
    {
        _next.Left = Width - 160;
        _next.Top = Height - 60;
        _next.Click += (_, _) =>
        {
            if (_step < 2) { _step++; Repaint(); return; }
            Theme.Set(_light.Checked ? ThemeMode.Light : ThemeMode.Dark);
            DialogResult = DialogResult.OK;
            Close();
        };

        _back.Left = Width - 262;
        _back.Top = Height - 60;
        _back.Visible = false;
        _back.Click += (_, _) =>
        {
            if (_step == 2) _pipeline.Reset();
            _step = Math.Max(0, _step - 1);
            Repaint();
        };

        _light.Left = 48;
        _light.Top = 210;
        _light.Width = 340;
        _light.Visible = false;

        _vibrance.Left = 48;
        _vibrance.Top = 210;
        _vibrance.Width = 400;
        _vibrance.Visible = false;
        _vibrance.Value = 50;
        _vibrance.ValueChanged += (_, _) => _pipeline.Apply(_vibrance.Value, 100, ToneState.Neutral);

        Controls.Add(_back);
        Controls.Add(_next);
        Controls.Add(_light);
        Controls.Add(_vibrance);
    }

    private void Repaint()
    {
        _back.Visible = _step > 0;
        _light.Visible = _step == 1;
        _vibrance.Visible = _step == 2;
        _next.Text = _step switch { 0 => "Get started", 1 => "Next", _ => "Finish" };
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        var p = Theme.Palette;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(p.Surface);

        using var border = new Pen(p.Border);
        g.DrawRectangle(border, 0, 0, Width - 1, Height - 1);

        using var dot = new SolidBrush(p.Accent);
        g.FillEllipse(dot, 48, 44, 14, 14);
        Draw.Text(g, Branding.Name, Theme.Title, p.Text, new Rectangle(72, 38, 380, 34));

        switch (_step)
        {
            case 0:
                Draw.Text(g, Branding.Tagline, Theme.Body, p.TextMuted, new Rectangle(48, 104, Width - 96, 24));
                Draw.Text(g, "No account, no sign-in. Everything runs locally:", Theme.Body, p.Text,
                    new Rectangle(48, 146, Width - 96, 24));
                Draw.Text(g, "•  Digital vibrance through the NVIDIA driver", Theme.Body, p.TextMuted,
                    new Rectangle(48, 178, Width - 96, 22));
                Draw.Text(g, "•  Gamma, brightness, contrast and colour balance", Theme.Body, p.TextMuted,
                    new Rectangle(48, 204, Width - 96, 22));
                Draw.Text(g, "•  Crosshair gallery, game profiles, share codes", Theme.Body, p.TextMuted,
                    new Rectangle(48, 230, Width - 96, 22));
                break;

            case 1:
                Draw.Text(g, "Pick a look", Theme.H2, p.Text, new Rectangle(48, 104, Width - 96, 28));
                Draw.Text(g, "You can change this any time from the sidebar or Settings.",
                    Theme.Body, p.TextMuted, new Rectangle(48, 140, Width - 96, 24));
                break;

            case 2:
                Draw.Text(g, "See the difference", Theme.H2, p.Text, new Rectangle(48, 104, Width - 96, 28));
                Draw.Text(g, "Drag the slider — your screen changes live. We will reset it when you finish.",
                    Theme.Body, p.TextMuted, new Rectangle(48, 140, Width - 96, 24));
                break;
        }

        Draw.Text(g, $"Step {_step + 1} of 3", Theme.Small, p.TextMuted,
            new Rectangle(48, Height - 52, 200, 20));
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        // Never leave the screen over-vibrant because of a demo slider.
        _pipeline.Apply(_originalVibrance, 100, ToneState.Neutral);
        base.OnFormClosed(e);
    }
}