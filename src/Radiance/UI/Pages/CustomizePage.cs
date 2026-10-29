using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class CustomizePage : Page
{
    private readonly ShellForm _shell;
    private readonly SegmentedRow _align = new();
    private readonly ToggleRow _widgets = new() { Text = "Show widgets button" };
    private readonly ToggleRow _taskView = new() { Text = "Show Task View button" };
    private readonly ToggleRow _searchHidden = new() { Text = "Search as icon only" };
    private readonly ToggleRow _extensions = new() { Text = "Show file extensions" };
    private readonly ToggleRow _hidden = new() { Text = "Show hidden files" };
    private readonly Caption _status = new();
    private bool _loading = true;

    public override string Title => "Customize";
    public override string Subtitle => "Quick Windows shell tweaks. Explorer restarts to apply.";

    public CustomizePage(ShellForm shell)
    {
        _shell = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        var bar = stack.Add("Taskbar");
        bar.AddAll(_align, _widgets, _taskView, _searchHidden);

        var explorer = stack.Add("File Explorer");
        explorer.AddAll(_extensions, _hidden);

        var actions = stack.AddRow(72);
        var apply = new FlatButton { Text = "Apply", Width = 110, Height = 34 };
        var restart = new FlatButton { Text = "Apply and restart Explorer", Accent = false, Width = 210, Height = 34, Left = 120 };
        apply.Click += (_, _) => Apply(restartExplorer: false);
        restart.Click += (_, _) => Apply(restartExplorer: true);
        actions.Controls.Add(restart);
        actions.Controls.Add(apply);

        _status.Left = 0;
        _status.Top = 44;
        _status.Width = 560;
        actions.Controls.Add(_status);

        _align.SetItems(new[] { ("left", "Left"), ("centre", "Centre") });

        LoadState();
        _align.SelectionChanged += (_, _) => Save();
        _widgets.CheckedChanged += (_, _) => Save();
        _taskView.CheckedChanged += (_, _) => Save();
        _searchHidden.CheckedChanged += (_, _) => Save();
        _extensions.CheckedChanged += (_, _) => Save();
        _hidden.CheckedChanged += (_, _) => Save();
    }

    private void LoadState()
    {
        var c = _shell.Settings.Customize;
        _loading = true;
        _align.Select(c.TaskbarLeftAlignment ? "left" : "centre");
        _widgets.Checked = c.ShowWidgets;
        _taskView.Checked = c.ShowTaskView;
        _searchHidden.Checked = c.HideSearchBox;
        _extensions.Checked = c.ShowFileExtensions;
        _hidden.Checked = c.ShowHiddenFiles;
        _loading = false;
    }

    private void Save()
    {
        if (_loading) return;

        var c = _shell.Settings.Customize;
        c.TaskbarLeftAlignment = _align.Selected == "left";
        c.ShowWidgets = _widgets.Checked;
        c.ShowTaskView = _taskView.Checked;
        c.HideSearchBox = _searchHidden.Checked;
        c.ShowFileExtensions = _extensions.Checked;
        c.ShowHiddenFiles = _hidden.Checked;
        _shell.SaveSettings();
    }

    private void Apply(bool restartExplorer)
    {
        Save();

        try
        {
            CustomizeRegistry.Apply(_shell.Settings.Customize);
            if (restartExplorer) CustomizeRegistry.RestartExplorer();

            _status.Text = restartExplorer
                ? "Applied. Explorer restarted."
                : "Applied. Restart Explorer to see the change.";
            _status.Ink = null;
        }
        catch (Exception ex)
        {
            _status.Text = $"Could not apply: {ex.Message}";
            _status.Ink = Color.FromArgb(0xE0, 0x9A, 0x4A);
        }
        _status.Invalidate();
    }

    public override void OnShown() => LoadState();
}
