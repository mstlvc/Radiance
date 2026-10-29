using Radiance.Core;
using Radiance.UI;
using Radiance.UI.Pages;

namespace Radiance;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        if (args.Any(a => a.Equals("--code-test", StringComparison.OrdinalIgnoreCase)))
        {
            var original = new Profile
            {
                Vibrance = 73, Saturation = 142, Gamma = 112, Brightness = 96, Contrast = 118,
                Temperature = -27, Red = 108, Green = 94, Blue = 121, Hue = 34,
                Blacks = -12, Whites = 9, Shadows = 21, Highlights = -8,
                ShadowTint = -15, HighlightTint = 22,
                ColourVisionMode = "green-weak", ColourVisionCorrection = true,
                EyeCareWarmth = 40, EyeCareDim = 15
            };

            string look = ShareCodes.EncodeLook(original);
            bool ok = ShareCodes.TryDecodeLook(look, out var back);

            Console.WriteLine("look code   : " + look);
            Console.WriteLine("decoded ok  : " + ok);

            if (ok)
            {
                var diffs = new List<string>();
                void Check(string name, object a, object b) { if (!Equals(a, b)) diffs.Add($"{name}: {a} != {b}"); }

                Check("Vibrance", original.Vibrance, back.Vibrance);
                Check("Saturation", original.Saturation, back.Saturation);
                Check("Gamma", original.Gamma, back.Gamma);
                Check("Brightness", original.Brightness, back.Brightness);
                Check("Contrast", original.Contrast, back.Contrast);
                Check("Temperature", original.Temperature, back.Temperature);
                Check("Red", original.Red, back.Red);
                Check("Green", original.Green, back.Green);
                Check("Blue", original.Blue, back.Blue);
                Check("Hue", original.Hue, back.Hue);
                Check("Blacks", original.Blacks, back.Blacks);
                Check("Whites", original.Whites, back.Whites);
                Check("Shadows", original.Shadows, back.Shadows);
                Check("Highlights", original.Highlights, back.Highlights);
                Check("ShadowTint", original.ShadowTint, back.ShadowTint);
                Check("HighlightTint", original.HighlightTint, back.HighlightTint);
                Check("Vision", original.ColourVisionMode, back.ColourVisionMode);
                Check("VisionCorrect", original.ColourVisionCorrection, back.ColourVisionCorrection);
                Check("EyeCareWarmth", original.EyeCareWarmth, back.EyeCareWarmth);
                Check("EyeCareDim", original.EyeCareDim, back.EyeCareDim);

                Console.WriteLine(diffs.Count == 0 ? "all 20 fields match" : "MISMATCH: " + string.Join("; ", diffs));
            }

            string bad = look[..^2] + (look[^1] == '2' ? "3" : "2");
            Console.WriteLine("corrupt code rejected: " + !ShareCodes.TryDecodeLook(bad, out _));

            var cross = new CrosshairSettings { Shape = "circle", Length = 18, Thickness = 3, Gap = 5, Opacity = 80, Dot = true, Outline = true, OutlineThickness = 2, Diagonal = true, ArmLeft = false, ColourArgb = Color.Cyan.ToArgb() };
            string cc = ShareCodes.EncodeCrosshair(cross);
            bool cok = ShareCodes.TryDecodeCrosshair(cc, out var cback);
            Console.WriteLine("crosshair   : " + cc);
            Console.WriteLine(cok && cback.Shape == cross.Shape && cback.Length == cross.Length && cback.Opacity == cross.Opacity
                && cback.Diagonal && !cback.ArmLeft && cback.ColourArgb == cross.ColourArgb
                ? "crosshair round-trip matches" : "crosshair MISMATCH");
            return;
        }

        if (args.Any(a => a.Equals("--report", StringComparison.OrdinalIgnoreCase)))
        {
            var loaded = AppSettings.Load();
            Console.WriteLine(Diagnostics.CreateBundle(loaded, "Generated from the command line."));
            return;
        }

        if (args.Any(a => a.Equals("--test-hdr", StringComparison.OrdinalIgnoreCase)))
        {
            var before = Hdr.Read();
            Console.WriteLine("before: " + string.Join("; ", before.Select(h => $"#{h.Index} supported={h.Supported} enabled={h.Enabled}")));

            bool on = Hdr.SetEnabled(1, true);
            Thread.Sleep(1500);
            Console.WriteLine("enable  -> " + on + " : " + string.Join("; ", Hdr.Read().Select(h => $"#{h.Index} enabled={h.Enabled}")));

            bool off = Hdr.SetEnabled(1, false);
            Thread.Sleep(1500);
            Console.WriteLine("disable -> " + off + " : " + string.Join("; ", Hdr.Read().Select(h => $"#{h.Index} enabled={h.Enabled}")));
            return;
        }

        if (args.Any(a => a.Equals("--test-timing", StringComparison.OrdinalIgnoreCase)))
        {
            var display = DisplayService.Displays().FirstOrDefault();
            if (display is null) { Console.WriteLine("no display"); return; }

            Console.WriteLine($"current: {display.DeviceName} {display.Width}x{display.Height}@{display.RefreshHz}");
            var applied = CustomTiming.TryApply(display.DeviceName, display.Width, display.Height, display.RefreshHz);
            Console.WriteLine($"apply   -> {applied.Ok}: {applied.Message}");

            Thread.Sleep(1500);
            bool reverted = CustomTiming.Revert(display.DeviceName);
            Console.WriteLine("revert  -> " + reverted);
            return;
        }

        if (args.Any(a => a.Equals("--reset", StringComparison.OrdinalIgnoreCase)))
        {
            using var pipeline = new ColorPipeline();
            pipeline.Initialize();
            pipeline.Reset();
            Console.WriteLine("colours reset to driver defaults");
            return;
        }

        if (args.Any(a => a.Equals("--gallery-test", StringComparison.OrdinalIgnoreCase)))
        {
            ApplicationConfiguration.Initialize();
            int bad = 0;

            foreach (var design in CrosshairDesigns.All)
            {
                foreach (var size in new[] { new Size(190, 72), new Size(120, 72), new Size(70, 72), new Size(60, 40), new Size(40, 30) })
                {
                    using var tile = new UI.DesignTile { Design = design, Width = size.Width, Height = size.Height };
                    using var bmp = new Bitmap(Math.Max(1, size.Width), Math.Max(1, size.Height));
                    try
                    {
                        tile.DrawToBitmap(bmp, new Rectangle(0, 0, bmp.Width, bmp.Height));
                    }
                    catch (Exception ex)
                    {
                        bad++;
                        Console.WriteLine($"FAIL {design.Id} @ {size.Width}x{size.Height}: {ex.GetType().Name}: {ex.Message}");
                    }
                }
            }

            Console.WriteLine(bad == 0 ? "all designs render at every size" : $"{bad} failures");
            return;
        }

        if (args.Any(a => a.Equals("--gamma-check", StringComparison.OrdinalIgnoreCase)))
        {
            int d = GammaRamp.ReadBackDeviation();
            Console.WriteLine(d < 0
                ? "gamma ramp unreadable"
                : $"gamma ramp deviation = {d} (0 = neutral, high = a look is applied)");
            return;
        }

        if (args.Any(a => a.Equals("--gpu", StringComparison.OrdinalIgnoreCase)))
        {
            using var nv = new NvidiaVibrance();
            bool ok = nv.Initialize();
            Console.WriteLine(ok
                ? $"NVIDIA available, {nv.DisplayCount} display(s), current level {nv.ReadCurrent()}, default {nv.ReadDefault()}"
                : "NVIDIA unavailable: " + (nv.LastError ?? "no NVIDIA GPU"));
            return;
        }

        if (args.Any(a => a.Equals("--monitors", StringComparison.OrdinalIgnoreCase)))
        {
            var monitors = MonitorHardware.Open();
            Console.WriteLine($"physical monitors: {monitors.Count}");
            if (monitors.Count == 0) Console.WriteLine("  reason: " + (MonitorHardware.LastError ?? "none"));
            foreach (var m in monitors)
            {
                bool ddc = MonitorHardware.Supports(m);
                var b = MonitorHardware.ReadBrightness(m);
                var c = MonitorHardware.ReadContrast(m);
                Console.WriteLine($"  {(string.IsNullOrWhiteSpace(m.Description) ? "(unnamed)" : m.Description)}"
                    + $"  DDC={ddc}"
                    + (b is null ? "  brightness=?" : $"  brightness={b.Value.Cur}/{b.Value.Max}")
                    + (c is null ? "  contrast=?" : $"  contrast={c.Value.Cur}"));
            }
            foreach (var line in MonitorHardware.Trace) Console.WriteLine("  trace: " + line);
            MonitorHardware.Close(monitors);

            foreach (var h in Hdr.Read())
                Console.WriteLine($"  HDR display {h.Index}: supported={h.Supported} enabled={h.Enabled}");
            return;
        }

        if (args.Any(a => a.Equals("--audio", StringComparison.OrdinalIgnoreCase)))
        {
            var v = AudioLimiter.GetVolume();
            Console.WriteLine(v is null ? "audio endpoint unavailable" : $"master volume = {v:P0}");
            return;
        }

        if (args.Any(a => a.Equals("--discord-send", StringComparison.OrdinalIgnoreCase)))
        {
            using var presence = new DiscordPresence(Branding.DiscordAppId);
            bool ok = presence.SetActivity(Environment.ProcessId, "Classic");
            Console.WriteLine(ok ? "activity accepted by Discord" : "failed: " + presence.LastError);
            Thread.Sleep(1200);
            return;
        }

        if (args.Any(a => a.Equals("--discord", StringComparison.OrdinalIgnoreCase)))
        {
            using var presence = new DiscordPresence(Branding.DiscordAppId);
            bool ok = presence.Connect();
            Console.WriteLine(ok ? "connected" : "not connected: " + presence.LastError);
            return;
        }

        if (args.Any(a => a.Equals("--shared", StringComparison.OrdinalIgnoreCase)))
        {
            foreach (var p in SharedPresets.Load())
                Console.WriteLine($"{p.Name}	{p.Vibrance}	{p.Gamma}");
            return;
        }

        if (args.Any(a => a.Equals("--screenshot", StringComparison.OrdinalIgnoreCase)))
        {
            Console.WriteLine(ColourScreenshot.Capture(AppSettings.Load()));
            return;
        }

        bool startInTray = args.Any(a =>
            a.Equals("--tray", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("-tray", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("/tray", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("--startup", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("-startup", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("/startup", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("--minimized", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("--minimised", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("-minimized", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("-minimised", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("/minimized", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("/minimised", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("--hidden", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("-hidden", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("--silent", StringComparison.OrdinalIgnoreCase) ||
            a.Equals("-silent", StringComparison.OrdinalIgnoreCase));

        // one instance per user session
        using var mutex = new Mutex(true, $"{Branding.Name}-single-instance", out bool first);
        if (!first)
        {
            if (!startInTray)
            {
                try
                {
                    if (EventWaitHandle.TryOpenExisting($"{Branding.Name}-show-event", out var evt))
                    {
                        evt.Set();
                        evt.Dispose();
                    }
                }
                catch { }
            }
            return;
        }

        using var showEvent = new EventWaitHandle(false, EventResetMode.AutoReset, $"{Branding.Name}-show-event");

        ApplicationConfiguration.Initialize();
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        if (Environment.GetEnvironmentVariable("RADIANCE_TRACE") == "1")
        {
            AppDomain.CurrentDomain.FirstChanceException += (_, e) =>
            {
                try
                {
                    if (e.Exception is ArgumentException)
                        File.AppendAllText(
                            Path.Combine(Path.GetTempPath(), "radiance-trace.log"),
                            e.Exception + Environment.NewLine + "-----" + Environment.NewLine);
                }
                catch { }
            };
        }

        Application.ThreadException += (_, e) => ShowCrash(e.Exception);
        AppDomain.CurrentDomain.UnhandledException += (_, e) => ShowCrash(e.ExceptionObject as Exception);

        var shell = new ShellForm(startInTray);
        var waitRegistration = ThreadPool.RegisterWaitForSingleObject(
            showEvent,
            (_, _) =>
            {
                try { shell.ShowMainWindow(); } catch { }
            },
            null,
            -1,
            false);

        Application.Run(shell);
        waitRegistration?.Unregister(null);
        GC.KeepAlive(mutex);
        GC.KeepAlive(showEvent);
    }

    private static void ShowCrash(Exception? ex)
    {
        MessageBox.Show(
            $"{Branding.Name} hit a problem but is still running.\n\n{ex?.Message}",
            Branding.Name, MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }
}
