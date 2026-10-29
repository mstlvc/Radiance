using System.Drawing.Drawing2D;
using Radiance.Core;

namespace Radiance.UI;

/// <summary>Tray presence: show the window, quick reset, quit.</summary>
public sealed class TrayIcon : IDisposable
{
    private readonly NotifyIcon _icon;
    private readonly ShellForm _shell;

    public bool Visible => _icon.Visible;

    public TrayIcon(ShellForm shell)
    {
        _shell = shell;

        _icon = new NotifyIcon
        {
            Icon = BrandAssets.AppIcon ?? BuildIcon(),
            Text = $"{Branding.Name} {Branding.Version}",
            Visible = true
        };

        var menu = new ContextMenuStrip();
        menu.Items.Add(new ToolStripMenuItem($"{Branding.Name} {Branding.Version}") { Enabled = false });
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Open main window", null, (_, _) => ShowWindow());
        menu.Items.Add("Reset colours", null, (_, _) => _shell.Pipeline.Reset());
        menu.Items.Add("Turn off per-pixel colour", null, (_, _) => _shell.DisableOverlay());
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Exit", null, (_, _) =>
        {
            _icon.Visible = false;
            _shell.ForceClose();
        });

        _icon.ContextMenuStrip = menu;
        _icon.DoubleClick += (_, _) => ShowWindow();
        _icon.MouseClick += (_, e) =>
        {
            if (e.Button == MouseButtons.Left) ShowWindow();
        };
    }

    private void ShowWindow() => _shell.ShowMainWindow();

    public void ShowProfile(string name)
    {
        _icon.ShowBalloonTip(1500, Branding.Name, $"{name} applied.", ToolTipIcon.None);
    }

    public void NotifyUpdate(UpdateInfo update)
    {
        _icon.ShowBalloonTip(6000, $"{Branding.Name} {update.Version} is available",
            "Click to open the release page.", ToolTipIcon.Info);
        _icon.BalloonTipClicked += (_, _) => Updater.Open(update.Url);
    }

    private static Icon BuildIcon()
    {
        var bmp = new Bitmap(32, 32);
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.Clear(Color.Transparent);

            using var ring = new Pen(Color.FromArgb(0x5B, 0x8C, 0xFF), 3f);
            g.DrawEllipse(ring, 5, 5, 22, 22);

            using var dot = new SolidBrush(Color.FromArgb(0x5B, 0x8C, 0xFF));
            g.FillEllipse(dot, 12, 12, 8, 8);
        }
        return Icon.FromHandle(bmp.GetHicon());
    }

    public void Dispose()
    {
        _icon.Visible = false;
        _icon.Dispose();
    }
}
