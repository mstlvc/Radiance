using Radiance.Core;

namespace Radiance.UI.Pages;

public sealed class ProfilesPage : Page
{
    private readonly ShellForm _shell;
    private readonly Card _gamesCard;
    private readonly Card _profilesCard;
    private readonly ToggleRow _autoApply = new() { Text = "Apply a game's profile while it runs" };
    private readonly Caption _status = new();
    private readonly TextBox _name = new();
    private readonly System.Windows.Forms.Timer _watcher = new() { Interval = 3000 };

    private List<Game> _games = new();
    private Dictionary<string, string> _map = new();
    private List<Profile> _profiles = new();
    private string? _selectedGame;
    private ListRow? _selectedProfileRow;
    private string? _appliedFor;

    public override string Title => "Profiles";
    public override string Subtitle => "Save looks and apply them automatically per game.";

    public ProfilesPage(ShellForm shell)
    {
        _shell = shell;
        _map = GameProfileStore.Load();
        _profiles = ProfileStore.Load();

        var stack = new CardStack();
        Controls.Add(stack);

        _gamesCard = stack.Add("Games", "Read from your Steam and Epic libraries.");
        _gamesCard.Add(_autoApply);

        _profilesCard = stack.Add("Saved profiles");

        var actions = stack.AddRow(96);
        _name.Left = 0;
        _name.Top = 4;
        _name.Width = 240;
        _name.Height = 28;
        _name.Text = "My look";
        _name.Font = Theme.Body;
        _name.BackColor = Theme.Palette.SurfaceAlt;
        _name.ForeColor = Theme.Palette.Text;
        _name.BorderStyle = BorderStyle.FixedSingle;
        actions.Controls.Add(_name);

        var save = new FlatButton { Text = "Save current", Accent = false, Width = 120, Height = 34, Left = 252, Top = 1 };
        save.Click += (_, _) => SaveCurrent();
        actions.Controls.Add(save);

        _status.Left = 0;
        _status.Top = 44;
        _status.Width = 560;
        actions.Controls.Add(_status);

        var toolbar = stack.AddRow(44);
        var apply = new FlatButton { Text = "Apply", Width = 96, Height = 34 };
        apply.Click += (_, _) => ApplySelected();
        var delete = new FlatButton { Text = "Delete", Accent = false, Width = 96, Height = 34, Left = 106 };
        delete.Click += (_, _) => DeleteSelected();
        toolbar.Controls.Add(apply);
        toolbar.Controls.Add(delete);

        _watcher.Tick += (_, _) => WatchGames();
        _watcher.Start();

        Reload();
    }

    private void Reload()
    {
        _games = Games.Detect().ToList();
        _profiles = ProfileStore.Load();
        _map = GameProfileStore.Load();
        BuildGames();
        BuildProfiles();
    }

    private void BuildGames()
    {
        // keep the auto-apply toggle as the first child
        for (int i = _gamesCard.Controls.Count - 1; i >= 0; i--)
            if (_gamesCard.Controls[i] is not ToggleRow) _gamesCard.Controls.RemoveAt(i);

        if (_games.Count == 0)
        {
            _gamesCard.Add(new Caption { Text = "No Steam or Epic games found." });
            return;
        }

        foreach (var game in _games)
        {
            string assigned = _map.TryGetValue(game.Name, out var p) ? p : "No profile";
            var row = new ListRow { Text = game.Name, Detail = $"{game.Source} · {assigned}" };
            row.Click += (_, _) =>
            {
                _selectedGame = game.Name;
                foreach (Control c in _gamesCard.Controls)
                    if (c is ListRow lr) lr.Selected = lr == row;
                _status.Text = $"Selected “{game.Name}”.";
            };
            _gamesCard.Add(row);
        }
    }

    private void BuildProfiles()
    {
        _profilesCard.Controls.Clear();
        _selectedProfileRow = null;

        foreach (var profile in _profiles)
        {
            var row = new ListRow
            {
                Text = profile.Name,
                Detail = $"Vibrance {profile.Vibrance} · Gamma {profile.Gamma} · Temp {profile.Temperature}"
            };
            row.Click += (_, _) =>
            {
                _selectedProfileRow = row;
                foreach (Control c in _profilesCard.Controls)
                    if (c is ListRow lr) lr.Selected = lr == row;
            };
            row.Activated += (_, _) => { _selectedProfileRow = row; ApplySelected(); };
            _profilesCard.Add(row);
        }
    }

    private Profile? SelectedProfile()
    {
        if (_selectedProfileRow is null) return null;
        int index = 0;
        foreach (Control c in _profilesCard.Controls)
        {
            if (c == _selectedProfileRow) break;
            index++;
        }
        return index < _profiles.Count ? _profiles[index] : null;
    }

    private void ApplySelected()
    {
        var p = SelectedProfile();
        if (p is null) { _status.Text = "Pick a profile first."; return; }

        _shell.Pipeline.Apply(p);

        var s = _shell.Settings;
        s.Vibrance = p.Vibrance; s.Saturation = p.Saturation; s.Gamma = p.Gamma;
        s.Brightness = p.Brightness; s.Contrast = p.Contrast; s.Temperature = p.Temperature;

        _status.Text = $"Applied “{p.Name}”.";
    }

    private void SaveCurrent()
    {
        string name = string.IsNullOrWhiteSpace(_name.Text) ? "My look" : _name.Text.Trim();
        var s = _shell.Settings;

        var profile = _profiles.FirstOrDefault(p =>
            string.Equals(p.Name, name, StringComparison.OrdinalIgnoreCase));

        if (profile is null) { profile = new Profile { Name = name }; _profiles.Add(profile); }

        profile.Vibrance = s.Vibrance;
        profile.Saturation = s.Saturation;
        profile.Gamma = s.Gamma;
        profile.Brightness = s.Brightness;
        profile.Contrast = s.Contrast;
        profile.Temperature = s.Temperature;

        ProfileStore.Save(_profiles);

        if (_selectedGame is not null)
        {
            _map[_selectedGame] = name;
            GameProfileStore.Save(_map);
            _status.Text = $"“{_selectedGame}” now uses “{name}”.";
        }
        else
        {
            _status.Text = $"Saved “{name}”.";
        }

        Reload();
    }

    private void DeleteSelected()
    {
        var p = SelectedProfile();
        if (p is null) { _status.Text = "Pick a profile first."; return; }

        _profiles.Remove(p);
        ProfileStore.Save(_profiles);

        foreach (var key in _map.Where(kv => kv.Value == p.Name).Select(kv => kv.Key).ToList())
            _map.Remove(key);
        GameProfileStore.Save(_map);

        _status.Text = $"Deleted “{p.Name}”.";
        Reload();
    }

    private void WatchGames()
    {
        if (!_autoApply.Checked) return;

        foreach (var game in _games)
        {
            if (!_map.TryGetValue(game.Name, out var profileName)) continue;
            if (_appliedFor == game.Name) continue;

            if (Games.IsRunning(game))
            {
                var profile = _profiles.FirstOrDefault(p =>
                    string.Equals(p.Name, profileName, StringComparison.OrdinalIgnoreCase));
                if (profile is null) continue;

                _shell.Pipeline.Apply(profile);
                _appliedFor = game.Name;
                _status.Text = $"Applied “{profile.Name}” for {game.Name}.";
                break;
            }
        }

        // reset when nothing mapped is running
        if (_appliedFor is not null &&
            !_games.Where(g => _map.ContainsKey(g.Name)).Any(Games.IsRunning))
        {
            _appliedFor = null;
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _watcher.Stop(); _watcher.Dispose(); }
        base.Dispose(disposing);
    }
}
