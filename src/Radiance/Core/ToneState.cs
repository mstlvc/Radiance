namespace Radiance.Core;

/// <summary>
/// Everything the gamma-ramp stage applies. All values are percent-style:
/// 100 is neutral for gain/brightness/contrast/gamma, 0 is neutral for
/// temperature, tints, blacks and whites.
/// </summary>
public readonly record struct ToneState(
    int Gamma = 100,
    int Brightness = 100,
    int Contrast = 100,
    int Temperature = 0,
    int Red = 100,
    int Green = 100,
    int Blue = 100,
    int Hue = 0,
    int Blacks = 0,
    int Whites = 0,
    int Shadows = 0,
    int Highlights = 0,
    int ShadowTint = 0,
    int HighlightTint = 0)
{
    public static ToneState Neutral => new();
}