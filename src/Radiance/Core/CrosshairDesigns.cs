namespace Radiance.Core;

/// <summary>Named crosshair designs, grouped into the same families as the original app.</summary>
public sealed record CrosshairDesign(string Id, string Name, string Family, CrosshairSettings Settings);

public static class CrosshairDesigns
{
    private static CrosshairSettings Make(string shape, int len, int thick, int gap,
        bool dot, Color colour, bool outline = false) => new()
    {
        Shape = shape,
        Length = len,
        Thickness = thick,
        Gap = gap,
        Dot = dot,
        Outline = outline,
        Opacity = 100,
        ColourArgb = colour.ToArgb()
    };

    /// <summary>Family order matches the original app's selector.</summary>
    public static IReadOnlyList<string> Families { get; } = new[]
    {
        "Dot", "X", "Ring", "Cross", "Funny"
    };

    public static IReadOnlyList<CrosshairDesign> All { get; } = new List<CrosshairDesign>
    {
        // Dot
        new("dot-tiny",     "Tiny dot",      "Dot", Make("dot", 10, 1, 0, false, Color.White)),
        new("dot-small",    "Small dot",     "Dot", Make("dot", 10, 2, 0, false, Color.Lime)),
        new("dot-medium",   "Medium dot",    "Dot", Make("dot", 10, 4, 0, false, Color.Red)),
        new("dot-bold",     "Bold dot",      "Dot", Make("dot", 10, 6, 0, false, Color.Cyan)),

        // X
        new("x-small",      "Small X",       "X", Make("x", 6, 1, 4, false, Color.Orange)),
        new("x-classic",    "Classic X",     "X", Make("x", 10, 2, 6, true, Color.Lime)),
        new("x-bold",       "Bold X",        "X", Make("x", 12, 3, 5, true, Color.Red)),
        new("x-wide",       "Wide X",        "X", Make("x", 16, 2, 8, false, Color.Cyan)),

        // Ring
        new("ring-tight",   "Tight ring",    "Ring", Make("circle", 12, 2, 2, true, Color.Cyan)),
        new("ring-thin",    "Thin ring",     "Ring", Make("circle", 14, 1, 4, false, Color.White)),
        new("ring-wide",    "Wide ring",     "Ring", Make("circle", 22, 2, 6, true, Color.Yellow)),
        new("ring-outline", "Outlined ring", "Ring", Make("circle", 16, 2, 4, true, Color.White, outline: true)),

        // Cross
        new("classic",      "Classic",       "Cross", Make("cross", 10, 2, 6, true, Color.Lime)),
        new("classic-line", "Outlined",      "Cross", Make("cross", 10, 2, 6, true, Color.Lime, outline: true)),
        new("mini",         "Mini",          "Cross", Make("cross", 5, 1, 3, false, Color.White)),
        new("wide",         "Wide",          "Cross", Make("cross", 16, 2, 8, false, Color.Cyan)),
        new("red-outline",  "Red on black",  "Cross", Make("cross", 12, 2, 6, true, Color.Red, outline: true)),

        // Funny
        new("t-shape",      "T shape",       "Funny", Make("t", 10, 2, 6, true, Color.Lime)),
        new("pink",         "Hot pink",      "Funny", Make("cross", 10, 2, 6, true, Color.HotPink)),
        new("magenta",      "Magenta",       "Funny", Make("cross", 10, 2, 6, true, Color.Magenta)),
        new("orange",       "Orange",        "Funny", Make("cross", 10, 2, 6, true, Color.Orange)),
    };

    public static CrosshairDesign? Match(CrosshairSettings settings)
    {
        foreach (var d in All)
        {
            var s = d.Settings;
            if (s.Shape == settings.Shape &&
                s.Length == settings.Length &&
                s.Thickness == settings.Thickness &&
                s.Gap == settings.Gap &&
                s.Dot == settings.Dot &&
                s.Outline == settings.Outline &&
                s.ColourArgb == settings.ColourArgb)
                return d;
        }
        return null;
    }

    public static void Apply(CrosshairSettings target, CrosshairDesign design)
    {
        target.Shape = design.Settings.Shape;
        target.Length = design.Settings.Length;
        target.Thickness = design.Settings.Thickness;
        target.Gap = design.Settings.Gap;
        target.Dot = design.Settings.Dot;
        target.Outline = design.Settings.Outline;
        target.Opacity = design.Settings.Opacity;
        target.ColourArgb = design.Settings.ColourArgb;
    }
}