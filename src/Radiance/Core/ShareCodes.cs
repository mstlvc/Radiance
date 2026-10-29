using System.Text;

namespace Radiance.Core;

/// <summary>
/// Copy-pasteable configuration codes. Everything is encoded locally — no
/// server, no account. Each code carries a version and a checksum so a
/// mistyped or truncated code is rejected instead of half-applying.
///
///   RDC-…   a complete colour look
///   RDCX-…  a crosshair
///   RDCM-…  a display mode
///   RDCF-…  a combination ("look.crosshair.mode", empty parts allowed)
/// </summary>
public static class ShareCodes
{
    private const string Alphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"; // exactly 32, no I/O
    private const byte LookVersion = 2;
    private const byte CrosshairVersion = 2;

    public const string LookPrefix = "RDC-";
    public const string CrosshairPrefix = "RDCX-";
    public const string ModePrefix = "RDCM-";
    public const string BundlePrefix = "RDCF-";

    private static readonly string[] VisionModes = { "normal", "red-weak", "green-weak", "blue-weak" };

    // ---------------------------------------------------------------- look

    public static string EncodeLook(Profile p)
    {
        var w = new Writer();
        w.Byte(LookVersion);

        w.Byte(p.Vibrance);            // 0..100
        w.Short(p.Saturation);         // 0..300
        w.Byte(p.Gamma - 50);          // 50..150
        w.Short(p.Brightness - 50);    // 50..170
        w.Byte(p.Contrast - 50);       // 50..150
        w.Byte(p.Temperature + 100);   // -100..100
        w.Byte(p.Red - 50);
        w.Byte(p.Green - 50);
        w.Byte(p.Blue - 50);
        w.Short(p.Hue + 180);          // -180..180
        w.Byte(p.Blacks + 100);
        w.Byte(p.Whites + 100);
        w.Byte(p.Shadows + 100);
        w.Byte(p.Highlights + 100);
        w.Byte(p.ShadowTint + 100);
        w.Byte(p.HighlightTint + 100);

        int vision = Array.IndexOf(VisionModes, p.ColourVisionMode ?? "normal");
        w.Byte((byte)Math.Max(0, vision));
        w.Byte((byte)(p.ColourVisionCorrection ? 1 : 0));

        w.Byte(p.EyeCareWarmth);
        w.Byte(p.EyeCareDim);

        return LookPrefix + Finish(w);
    }

    public static bool TryDecodeLook(string? code, out Profile profile)
    {
        profile = new Profile();
        if (!Strip(code, LookPrefix, out var body)) return false;

        var r = Open(body);
        if (r is null) return false;

        try
        {
            if (r.Byte() != LookVersion) return false;

            profile.Vibrance = r.Byte();
            profile.Saturation = r.Short();
            profile.Gamma = r.Byte() + 50;
            profile.Brightness = r.Short() + 50;
            profile.Contrast = r.Byte() + 50;
            profile.Temperature = r.Byte() - 100;
            profile.Red = r.Byte() + 50;
            profile.Green = r.Byte() + 50;
            profile.Blue = r.Byte() + 50;
            profile.Hue = r.Short() - 180;
            profile.Blacks = r.Byte() - 100;
            profile.Whites = r.Byte() - 100;
            profile.Shadows = r.Byte() - 100;
            profile.Highlights = r.Byte() - 100;
            profile.ShadowTint = r.Byte() - 100;
            profile.HighlightTint = r.Byte() - 100;

            int vision = Math.Clamp((int)r.Byte(), 0, VisionModes.Length - 1);
            profile.ColourVisionMode = VisionModes[vision];
            profile.ColourVisionCorrection = r.Byte() != 0;

            profile.EyeCareWarmth = r.Byte();
            profile.EyeCareDim = r.Byte();

            return r.Finished;
        }
        catch { return false; }
    }

    // ----------------------------------------------------------- crosshair

    public static string EncodeCrosshair(CrosshairSettings c)
    {
        var w = new Writer();
        w.Byte(CrosshairVersion);

        w.Byte((byte)ShapeIndex(c.Shape));
        w.Byte((byte)Math.Clamp(c.Length, 0, 255));
        w.Byte((byte)Math.Clamp(c.Thickness, 0, 255));
        w.Byte((byte)Math.Clamp(c.Gap, 0, 255));
        w.Byte((byte)Math.Clamp(c.Opacity, 0, 100));
        w.Byte((byte)Math.Clamp(c.OutlineThickness, 0, 255));

        byte flags = 0;
        if (c.Dot) flags |= 1;
        if (c.Outline) flags |= 2;
        if (c.Diagonal) flags |= 4;
        if (c.ArmTop) flags |= 8;
        if (c.ArmBottom) flags |= 16;
        if (c.ArmLeft) flags |= 32;
        if (c.ArmRight) flags |= 64;
        w.Byte(flags);

        w.Byte((byte)((c.ColourArgb >> 16) & 0xFF));
        w.Byte((byte)((c.ColourArgb >> 8) & 0xFF));
        w.Byte((byte)(c.ColourArgb & 0xFF));

        return CrosshairPrefix + Finish(w);
    }

    public static bool TryDecodeCrosshair(string? code, out CrosshairSettings cfg)
    {
        cfg = new CrosshairSettings();
        if (!Strip(code, CrosshairPrefix, out var body)) return false;

        var r = Open(body);
        if (r is null) return false;

        try
        {
            if (r.Byte() != CrosshairVersion) return false;

            cfg.Shape = ShapeName(r.Byte());
            cfg.Length = Math.Clamp((int)r.Byte(), 2, 40);
            cfg.Thickness = Math.Clamp((int)r.Byte(), 1, 8);
            cfg.Gap = Math.Clamp((int)r.Byte(), 0, 30);
            cfg.Opacity = Math.Clamp((int)r.Byte(), 20, 100);
            cfg.OutlineThickness = Math.Clamp((int)r.Byte(), 1, 4);

            byte flags = r.Byte();
            cfg.Dot = (flags & 1) != 0;
            cfg.Outline = (flags & 2) != 0;
            cfg.Diagonal = (flags & 4) != 0;
            cfg.ArmTop = (flags & 8) != 0;
            cfg.ArmBottom = (flags & 16) != 0;
            cfg.ArmLeft = (flags & 32) != 0;
            cfg.ArmRight = (flags & 64) != 0;

            cfg.ColourArgb = Color.FromArgb(r.Byte(), r.Byte(), r.Byte()).ToArgb();
            return r.Finished;
        }
        catch { return false; }
    }

    // ------------------------------------------------------------ display

    public static string EncodeMode(DisplayMode mode)
    {
        var w = new Writer();
        w.Byte(1);
        w.Short(mode.Width);
        w.Short(mode.Height);
        w.Short(mode.RefreshHz);
        return ModePrefix + Finish(w);
    }

    public static bool TryDecodeMode(string? code, out DisplayMode mode)
    {
        mode = new DisplayMode(0, 0, 0);
        if (!Strip(code, ModePrefix, out var body)) return false;

        var r = Open(body);
        if (r is null) return false;

        try
        {
            if (r.Byte() != 1) return false;
            mode = new DisplayMode(r.Short(), r.Short(), r.Short());
            return r.Finished && mode.Width > 0 && mode.Height > 0;
        }
        catch { return false; }
    }

    // ------------------------------------------------------------- bundle

    public sealed record Bundle(Profile? Look, CrosshairSettings? Crosshair, DisplayMode? Mode);

    public static string Encode(Profile? look, CrosshairSettings? crosshair, DisplayMode? mode)
    {
        string a = look is null ? "" : EncodeLook(look);
        string b = crosshair is null ? "" : EncodeCrosshair(crosshair);
        string c = mode is null ? "" : EncodeMode(mode);

        if (a.Length + b.Length + c.Length == 0)
            throw new ArgumentException("A bundle needs at least one part.");

        return BundlePrefix + string.Join('.', a, b, c);
    }

    public static bool TryDecodeBundle(string? code, out Bundle bundle)
    {
        bundle = new Bundle(null, null, null);
        if (!Strip(code, BundlePrefix, out var body)) return false;

        var parts = body.Split('.');
        Profile? look = parts.Length > 0 && TryDecodeLook(parts[0], out var p) ? p : null;
        CrosshairSettings? cross = parts.Length > 1 && TryDecodeCrosshair(parts[1], out var c) ? c : null;
        DisplayMode? mode = parts.Length > 2 && TryDecodeMode(parts[2], out var m) ? m : null;

        bundle = new Bundle(look, cross, mode);
        return look is not null || cross is not null || mode is not null;
    }

    // ------------------------------------------------------------ helpers

    private static string Finish(Writer w)
    {
        w.Byte(Crc8(w.Data));
        return Base32(w.Data);
    }

    private static Reader? Open(string body)
    {
        var bytes = UnBase32(body);
        if (bytes.Length < 2) return null;

        byte expected = bytes[^1];
        var payload = bytes[..^1];

        // reject a mistyped code rather than applying half of it
        if (Crc8(payload) != expected) return null;

        return new Reader(payload);
    }

    private static byte Crc8(byte[] data)
    {
        byte crc = 0x00;
        foreach (byte b in data)
        {
            crc ^= b;
            for (int i = 0; i < 8; i++)
                crc = (byte)((crc & 0x80) != 0 ? (crc << 1) ^ 0x07 : crc << 1);
        }
        return crc;
    }

    private static bool Strip(string? code, string prefix, out string body)
    {
        body = "";
        if (string.IsNullOrWhiteSpace(code)) return false;

        string s = code.Trim().ToUpperInvariant().Replace("-", "");
        string p = prefix.Replace("-", "").ToUpperInvariant();

        if (!s.StartsWith(p, StringComparison.Ordinal)) return false;

        body = s[p.Length..];
        return body.Length > 0;
    }

    private static int ShapeIndex(string shape) => shape switch
    {
        "dot" => 1,
        "circle" => 2,
        "t" => 3,
        "x" => 4,
        _ => 0
    };

    private static string ShapeName(int i) => i switch
    {
        1 => "dot",
        2 => "circle",
        3 => "t",
        4 => "x",
        _ => "cross"
    };

    private static string Base32(byte[] data)
    {
        var sb = new StringBuilder((data.Length * 8 + 4) / 5);
        int buffer = 0, bits = 0;

        foreach (byte b in data)
        {
            buffer = (buffer << 8) | b;
            bits += 8;

            while (bits >= 5)
            {
                bits -= 5;
                sb.Append(Alphabet[(buffer >> bits) & 0x1F]);
            }

            // keep only the bits not yet emitted, so the accumulator cannot overflow
            buffer &= (1 << bits) - 1;
        }

        if (bits > 0)
            sb.Append(Alphabet[(buffer << (5 - bits)) & 0x1F]);

        return sb.ToString();
    }

    private static byte[] UnBase32(string s)
    {
        var bytes = new List<byte>(s.Length * 5 / 8);
        int buffer = 0, bits = 0;

        foreach (char ch in s)
        {
            int v = Alphabet.IndexOf(ch);
            if (v < 0) return Array.Empty<byte>();

            buffer = (buffer << 5) | v;
            bits += 5;
            if (bits >= 8)
            {
                bits -= 8;
                bytes.Add((byte)((buffer >> bits) & 0xFF));
                buffer &= (1 << bits) - 1;
            }
        }

        return bytes.ToArray();
    }

    private sealed class Writer
    {
        private readonly List<byte> _data = new();
        public byte[] Data => _data.ToArray();
        public void Byte(byte b) => _data.Add(b);
        public void Byte(int b) => _data.Add((byte)Math.Clamp(b, 0, 255));
        public void Short(int v)
        {
            ushort u = (ushort)Math.Clamp(v, 0, 65535);
            _data.Add((byte)(u & 0xFF));
            _data.Add((byte)(u >> 8));
        }
    }

    private sealed class Reader
    {
        private readonly byte[] _data;
        private int _at;

        public Reader(byte[] data) => _data = data;

        public bool Finished => _at >= _data.Length;

        public byte Byte()
        {
            if (_at >= _data.Length) throw new InvalidOperationException("short code");
            return _data[_at++];
        }

        public int Short()
        {
            if (_at + 1 >= _data.Length) throw new InvalidOperationException("short code");
            int lo = _data[_at++], hi = _data[_at++];
            return lo | (hi << 8);
        }
    }
}