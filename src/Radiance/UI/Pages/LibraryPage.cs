using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class LibraryPage : Page
{
    private readonly ShellForm _shell;
    private readonly Card _card;
    private readonly Card _sharedCard;
    private readonly Caption _status = new();

    public override string Title => "Library";
    public override string Subtitle => "Starting points you can apply in one click.";

    public LibraryPage(ShellForm shell)
    {
        _shell = shell;

        var stack = new CardStack();
        Controls.Add(stack);

        _card = stack.Add("Presets", "Apply a preset, then fine-tune it on the Vibrance page.");

        _sharedCard = stack.Add("Shared presets",
            @"Files in %APPDATA%\Radiance\shared — no server involved.");

        var actions = stack.AddRow(44);
        var import = new FlatButton { Text = "Import…", Accent = false, Width = 110, Height = 32 };
        var openFolder = new FlatButton { Text = "Open folder", Accent = false, Width = 120, Height = 32, Left = 120 };
        import.Click += (_, _) => ImportShared();
        openFolder.Click += (_, _) => OpenSharedFolder();
        actions.Controls.Add(openFolder);
        actions.Controls.Add(import);

        stack.AddRow(28).Controls.Add(_status);

        Build();
    }

    private void Build()
    {
        _card.Controls.Clear();

        var flow = new FlowLayoutPanel
        {
            Height = 230,
            BackColor = Color.Transparent,
            WrapContents = true,
            AutoScroll = false
        };

        foreach (var preset in ProfileStore.BuiltIns())
        {
            var tile = new FlatButton
            {
                Text = preset.Name,
                Accent = false,
                Width = 150,
                Height = 64,
                Margin = new Padding(0, 0, 10, 10)
            };
            tile.Click += (_, _) =>
            {
                _shell.Pipeline.Apply(preset);

                var s = _shell.Settings;
                s.Vibrance = preset.Vibrance; s.Saturation = preset.Saturation; s.Gamma = preset.Gamma;
                s.Brightness = preset.Brightness; s.Contrast = preset.Contrast; s.Temperature = preset.Temperature;

                _status.Text = $"Applied “{preset.Name}”.";
                _status.Invalidate();
            };
            flow.Controls.Add(tile);
        }

        _card.Add(flow);
    }

    public override void OnShown()
    {
        BuildShared();
        _status.Text = "Presets are built in; your own ones live on the Profiles page.";
    }

    private void BuildShared()
    {
        _sharedCard.Controls.Clear();
        var shared = SharedPresets.Load();

        if (shared.Count == 0)
        {
            _sharedCard.Add(new Caption { Text = "No shared presets yet. Import one, or export from Profiles." });
            return;
        }

        foreach (var preset in shared)
        {
            var row = new ListRow
            {
                Text = preset.Name,
                Detail = $"Vibrance {preset.Vibrance} · Gamma {preset.Gamma} · Temp {preset.Temperature}"
            };
            row.Click += (_, _) =>
            {
                _shell.Pipeline.Apply(preset);
                var st = _shell.Settings;
                st.Vibrance = preset.Vibrance; st.Saturation = preset.Saturation; st.Gamma = preset.Gamma;
                st.Brightness = preset.Brightness; st.Contrast = preset.Contrast; st.Temperature = preset.Temperature;
                st.Red = preset.Red; st.Green = preset.Green; st.Blue = preset.Blue;
                st.Blacks = preset.Blacks; st.Highlights = preset.Highlights;
                _status.Text = $"Applied “{preset.Name}”.";
                _status.Invalidate();
            };
            _sharedCard.Add(row);
        }
    }

    private void ImportShared()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Import a shared preset",
            Filter = $"Radiance looks|*{SharedPresets.Extension};*.json|All files|*.*"
        };

        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        bool ok = SharedPresets.Import(dialog.FileName, out string message);
        _status.Text = message;
        _status.Ink = ok ? null : Color.FromArgb(0xE0, 0x9A, 0x4A);
        _status.Invalidate();
        BuildShared();
    }

    private void OpenSharedFolder()
    {
        try
        {
            Directory.CreateDirectory(SharedPresets.Folder);
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(SharedPresets.Folder)
            {
                UseShellExecute = true
            });
        }
        catch { }
    }
}
