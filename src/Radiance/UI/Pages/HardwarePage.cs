using Radiance.Core;

namespace Radiance.UI.Pages;

/// <summary>Hardware monitor control over DDC/CI, separate from the GPU gamma ramp.</summary>
public sealed class HardwarePage : Page
{
    private readonly List<MonitorHardware.PhysicalMonitor> _monitors = new();
    private readonly SegmentedRow _which = new();
    private readonly SliderRow _brightness = new() { Label = "Brightness", Min = 0, Max = 100 };
    private readonly SliderRow _contrast = new() { Label = "Contrast", Min = 0, Max = 100 };
    private readonly SliderRow _red = new() { Label = "Red gain", Min = 0, Max = 100 };
    private readonly SliderRow _green = new() { Label = "Green gain", Min = 0, Max = 100 };
    private readonly SliderRow _blue = new() { Label = "Blue gain", Min = 0, Max = 100 };
    private readonly SegmentedRow _input = new();
    private readonly Caption _status = new();
    private bool _loading = true;

    public override string Title => "Monitor hardware";
    public override string Subtitle => "DDC/CI brightness, contrast, gain and input source.";

    public HardwarePage(ShellForm shell)
    {
        _ = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        var pick = stack.Add("Monitor", "Not every monitor or cable supports DDC/CI.");
        pick.AddAll(_which, _status);

        var levels = stack.Add("Levels");
        levels.AddAll(_brightness, _contrast, _red, _green, _blue);

        var input = stack.Add("Input source");
        input.Add(_input);

        foreach (var s in new[] { _brightness, _contrast, _red, _green, _blue })
            s.ValueChanged += (_, _) => WriteLevels();

        _which.SelectionChanged += (_, _) => ReadLevels();
        _input.SelectionChanged += (_, _) => WriteInput();

        LoadState();
    }

    public override void OnShown() => LoadState();

    private void LoadState()
    {
        MonitorHardware.Close(_monitors);
        _monitors.Clear();
        _monitors.AddRange(MonitorHardware.Open());

        if (_monitors.Count == 0)
        {
            _which.SetItems(new[] { ("none", "No monitors") });
            _status.Text = "DDC/CI is not available on this system.";
            _status.Ink = Color.FromArgb(0xE0, 0x9A, 0x4A);
            return;
        }

        var items = _monitors.Select((m, i) =>
            (i.ToString(), string.IsNullOrWhiteSpace(m.Description) ? $"Display {i + 1}" : m.Description)).ToArray();

        _which.SetItems(items);
        _input.SetItems(MonitorHardware.InputSources.Select(s => (s.Code.ToString(), s.Name)));

        ReadLevels();
    }

    private MonitorHardware.PhysicalMonitor? Current()
    {
        if (!int.TryParse(_which.Selected, out int i)) return null;
        return i >= 0 && i < _monitors.Count ? _monitors[i] : null;
    }

    private void ReadLevels()
    {
        var m = Current();
        if (m is null) return;

        _loading = true;

        if (MonitorHardware.ReadBrightness(m) is { } b)
        {
            _brightness.Min = b.Min;
            _brightness.Max = b.Max;
            _brightness.Value = Math.Clamp(b.Cur, b.Min, b.Max);
        }

        if (MonitorHardware.ReadContrast(m) is { } c)
            _contrast.Value = Math.Clamp(c.Cur, 0, 100);

        if (MonitorHardware.ReadGain(m, "red") is { } r) _red.Value = Math.Clamp(r.Cur, 0, 100);
        if (MonitorHardware.ReadGain(m, "green") is { } g) _green.Value = Math.Clamp(g.Cur, 0, 100);
        if (MonitorHardware.ReadGain(m, "blue") is { } bl) _blue.Value = Math.Clamp(bl.Cur, 0, 100);

        _loading = false;

        bool supported = MonitorHardware.Supports(m);
        _status.Text = supported
            ? "This monitor accepts DDC/CI commands."
            : "This monitor did not answer DDC/CI. Levels may be read-only.";
        _status.Ink = supported ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
        _status.Invalidate();
    }

    private void WriteLevels()
    {
        if (_loading) return;

        var m = Current();
        if (m is null) return;

        MonitorHardware.WriteBrightness(m, _brightness.Value);
        MonitorHardware.WriteContrast(m, _contrast.Value);
        MonitorHardware.WriteGain(m, "red", _red.Value);
        MonitorHardware.WriteGain(m, "green", _green.Value);
        MonitorHardware.WriteGain(m, "blue", _blue.Value);
    }

    private void WriteInput()
    {
        if (_loading) return;
        if (!int.TryParse(_input.Selected, out int code)) return;

        var m = Current();
        if (m is null) return;

        MonitorHardware.WriteInput(m, code);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) MonitorHardware.Close(_monitors);
        base.Dispose(disposing);
    }
}