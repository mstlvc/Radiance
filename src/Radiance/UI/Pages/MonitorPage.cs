using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class MonitorPage : Page
{
    private readonly Card _displaysCard;
    private readonly Card _modesCard;
    private readonly Card _hdrCard;
    private readonly Card _scaleCard;
    private readonly SegmentedRow _scaling = new();
    private readonly TextBox _w = new();
    private readonly TextBox _h = new();
    private readonly TextBox _hz = new();
    private readonly Caption _status = new();
    private readonly List<DisplayInfo> _displays = new();
    private readonly List<DisplayMode> _modes = new();
    private readonly List<ListRow> _displayRows = new();
    private ListRow? _selectedMode;
    private int _selectedDisplay = -1;

    public override string Title => "Monitor";
    public override string Subtitle => "Resolution, refresh rate and HDR state.";

    public MonitorPage(ShellForm shell)
    {
        _ = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        _displaysCard = stack.Add("Displays");
        _modesCard = stack.Add("Resolution", "Double-click a mode to apply it.");
        _scaleCard = stack.Add("Scaling", "How a non-native resolution is fitted to the panel.");
        _scaling.SetItems(new[]
        {
            ("0", "Default"), ("2", "Stretch"), ("3", "Aspect"), ("1", "Centre")
        });
        _scaleCard.Add(_scaling);

        var custom = stack.Add("Custom resolution", "Applied through the display driver.");
        var customRow = new Panel { Height = 38, BackColor = Color.Transparent };
        foreach (var (box, label, left) in new[] { (_w, "W", 0), (_h, "H", 92), (_hz, "Hz", 184) })
        {
            box.Left = left;
            box.Top = 4;
            box.Width = 78;
            box.Height = 26;
            box.TextAlign = HorizontalAlignment.Center;
            box.BorderStyle = BorderStyle.FixedSingle;
            box.Font = Theme.Body;
            box.BackColor = Theme.Palette.SurfaceAlt;
            box.ForeColor = Theme.Palette.Text;
            box.PlaceholderText = label;
            customRow.Controls.Add(box);
        }
        var applyCustom = new FlatButton { Text = "Apply", Accent = false, Width = 90, Height = 30, Left = 274, Top = 2 };
        applyCustom.Click += (_, _) => ApplyCustom();
        customRow.Controls.Add(applyCustom);
        custom.Add(customRow);

        _hdrCard = stack.Add("HDR");

        var row = stack.AddRow(44);
        var apply = new FlatButton { Text = "Apply mode", Width = 130, Height = 34 };
        apply.Click += (_, _) => ApplyMode();
        row.Controls.Add(apply);
        row.Controls.Add(_status);
        _status.Left = 142;
        _status.Top = 6;
        _status.Width = 520;

        LoadDisplays();
    }

    public override void OnShown() => LoadDisplays();

    private void LoadDisplays()
    {
        _displaysCard.Controls.Clear();
        _displayRows.Clear();
        _displays.Clear();
        _displays.AddRange(DisplayService.Displays());

        for (int i = 0; i < _displays.Count; i++)
        {
            var d = _displays[i];
            var row = new ListRow
            {
                Text = $"{d.FriendlyName}",
                Detail = $"{d.Width} × {d.Height} @ {d.RefreshHz} Hz  ·  {d.DeviceName}"
            };
            int index = i;
            row.Click += (_, _) => SelectDisplay(index);
            _displaysCard.Add(row);
            _displayRows.Add(row);
        }

        if (_displayRows.Count > 0) SelectDisplay(0);
        LoadHdr();
    }

    private void SelectDisplay(int index)
    {
        _selectedDisplay = index;
        for (int i = 0; i < _displayRows.Count; i++)
            _displayRows[i].Selected = i == index;

        _modes.Clear();
        _modesCard.Controls.Clear();
        _selectedMode = null;

        if (index < 0 || index >= _displays.Count) return;

        _modes.AddRange(DisplayService.Modes(_displays[index].DeviceName));

        foreach (var mode in _modes.Take(14))
        {
            var row = new ListRow
            {
                Text = $"{mode.Width} × {mode.Height}",
                Detail = $"{mode.RefreshHz} Hz"
            };

            row.Click += (_, _) =>
            {
                _selectedMode = row;
                foreach (Control c in _modesCard.Controls)
                    if (c is ListRow lr) lr.Selected = lr == row;
            };
            row.Activated += (_, _) => { _selectedMode = row; ApplyMode(); };

            _modesCard.Add(row);
        }
    }

    private void ApplyMode()
    {
        if (_selectedDisplay < 0 || _selectedDisplay >= _displays.Count) return;

        int i = _modesCard.Controls.IndexOf(_selectedMode!);
        if (_selectedMode is null || i < 0) { _status.Text = "Pick a mode first."; return; }

        // index among ListRows, not raw control index
        int rowIndex = 0;
        foreach (Control c in _modesCard.Controls)
        {
            if (c == _selectedMode) break;
            if (c is ListRow) rowIndex++;
        }

        if (rowIndex >= _modes.Count) return;
        var mode = _modes[rowIndex];

        bool ok = DisplayService.SetMode(_displays[_selectedDisplay].DeviceName, mode);
        _status.Text = ok ? $"Applied {mode}." : "The display refused that mode.";
        _status.Ink = ok ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
        _status.Invalidate();

        if (ok) LoadDisplays();
    }

    private void ApplyScaling()
    {
        if (_selectedDisplay < 0 || _selectedDisplay >= _displays.Count) return;

        if (int.TryParse(_scaling.Selected, out int v))
        {
            bool ok = DisplayService.SetScaling(_displays[_selectedDisplay].DeviceName, (ScalingMode)v);
            _status.Text = ok ? "Scaling updated." : "The driver refused that scaling mode.";
            _status.Ink = ok ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
            _status.Invalidate();
        }
    }

    private void ApplyCustom()
    {
        if (_selectedDisplay < 0 || _selectedDisplay >= _displays.Count) return;

        if (!int.TryParse(_w.Text, out int w) ||
            !int.TryParse(_h.Text, out int h) ||
            !int.TryParse(_hz.Text, out int hz))
        {
            _status.Text = "Enter width, height and refresh rate.";
            return;
        }

        var (ok, message) = DisplayService.TryCustomMode(_displays[_selectedDisplay].DeviceName, w, h, hz);

        // If Windows refused it, ask the NVIDIA driver directly for a timing override.
        if (!ok)
        {
            var nv = CustomTiming.TryApply(_displays[_selectedDisplay].DeviceName, w, h, hz);
            ok = nv.Ok;
            message = nv.Message;
        }

        _status.Text = message;
        _status.Ink = ok ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
        _status.Invalidate();

        if (ok) LoadDisplays();
    }

    private void LoadHdr()
    {
        _hdrCard.Controls.Clear();
        var states = Hdr.Read();

        if (states.Count == 0)
        {
            _hdrCard.Add(new Caption { Text = "HDR state could not be read on this system." });
            return;
        }

        foreach (var s in states)
        {
            string text = !s.Supported
                ? $"Display {s.Index}: HDR not supported"
                : s.Enabled ? $"Display {s.Index}: HDR on" : $"Display {s.Index}: HDR capable, currently off";

            _hdrCard.Add(new Caption { Text = text });

            if (s.Supported)
            {
                var toggle = new FlatButton
                {
                    Text = s.Enabled ? "Turn HDR off" : "Turn HDR on",
                    Accent = false,
                    Width = 150,
                    Height = 30
                };
                int index = s.Index;
                bool enable = !s.Enabled;
                toggle.Click += (_, _) =>
                {
                    bool ok = Hdr.SetEnabled(index, enable);
                    _status.Text = ok ? "HDR state changed." : "Windows refused the HDR change.";
                    _status.Ink = ok ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
                    _status.Invalidate();
                    if (ok) LoadHdr();
                };
                _hdrCard.Add(toggle);
            }
        }
    }
}
