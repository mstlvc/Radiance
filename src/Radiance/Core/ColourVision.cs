namespace Radiance.Core;

/// <summary>
/// Colour-vision simulation and correction via LMS matrices.
/// "Simulate" shows what a colour-blind viewer sees; "correct" shifts colours
/// away from the confusion axis so they become distinguishable.
/// </summary>
public static class ColourVision
{
    public static readonly string[] Modes = { "normal", "red-weak", "green-weak", "blue-weak" };

    public static string Label(string mode) => mode switch
    {
        "red-weak" => "Red-weak (protanopia)",
        "green-weak" => "Green-weak (deuteranopia)",
        "blue-weak" => "Blue-weak (tritanopia)",
        _ => "Normal"
    };

    private static readonly double[,] RgbToLms =
    {
        { 17.8824, 43.5161, 4.11935 },
        { 3.45565, 27.1554, 3.86714 },
        { 0.0299566, 0.184309, 1.46709 }
    };

    private static readonly double[,] LmsToRgb =
    {
        {  0.0809444479, -0.130504409,  0.116721066 },
        { -0.0102485335,  0.0540193266, -0.113614708 },
        { -0.000365296938, -0.00412161469, 0.693511405 }
    };

    private static readonly double[,] ShiftProtan =
    {
        { 0, 0, 0 },
        { 0.7, 1, 0 },
        { 0.7, 0, 1 }
    };

    private static readonly double[,] ShiftDeutan =
    {
        { 1, 0.7, 0 },
        { 0, 1, 0 },
        { 0, 0.7, 1 }
    };

    private static readonly double[,] ShiftTritan =
    {
        { 1, 0, 0.7 },
        { 0, 1, 0.7 },
        { 0, 0, 1 }
    };

    private static readonly double[,] Identity =
    {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 }
    };

    private static double[,] ShiftFor(string mode) => mode switch
    {
        "red-weak" => ShiftProtan,
        "green-weak" => ShiftDeutan,
        "blue-weak" => ShiftTritan,
        _ => Identity
    };

    /// <summary>Simulated RGB->RGB matrix for a deficiency.</summary>
    public static double[,] SimulateMatrix(string mode)
    {
        var shift = ShiftFor(mode);
        if (ReferenceEquals(shift, Identity)) return Identity;
        return Multiply(Multiply(LmsToRgb, shift), RgbToLms);
    }

    /// <summary>
    /// Correction matrix: pushes confused colours apart instead of collapsing them.
    /// Derived as the inverse-style shift on the same axis.
    /// </summary>
    public static double[,] CorrectMatrix(string mode)
    {
        var shift = ShiftFor(mode);
        if (ReferenceEquals(shift, Identity)) return Identity;

        // inverse of the shift in LMS space: subtract the leaked channel
        var inverse = new double[3, 3];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                inverse[r, c] = (r == c ? 1.0 : 0.0) - (shift[r, c] * 0.6);

        return Multiply(Multiply(LmsToRgb, inverse), RgbToLms);
    }

    private static double[,] Multiply(double[,] a, double[,] b)
    {
        var r = new double[3, 3];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                for (int k = 0; k < 3; k++)
                    r[i, j] += a[i, k] * b[k, j];
        return r;
    }

    /// <summary>Applies a matrix to a normalised colour, clamping each channel.</summary>
    public static (double r, double g, double b) Apply(double[,] m, double r, double g, double b)
    {
        double nr = m[0, 0] * r + m[0, 1] * g + m[0, 2] * b;
        double ng = m[1, 0] * r + m[1, 1] * g + m[1, 2] * b;
        double nb = m[2, 0] * r + m[2, 1] * g + m[2, 2] * b;

        return (Math.Clamp(nr, 0, 1), Math.Clamp(ng, 0, 1), Math.Clamp(nb, 0, 1));
    }

    /// <summary>The matrix the pipeline should bake into the ramp, or null for none.</summary>
    public static double[,]? For(string mode, bool correction)
    {
        if (mode == "normal") return null;
        return correction ? CorrectMatrix(mode) : SimulateMatrix(mode);
    }
}