// The maths behind a look: the per-channel gamma ramp and the cross-channel
// colour matrix. Pure functions, no Windows - the colour engine sends the results
// to the hardware.
#pragma once

#include "core/Model.h"

#include <array>
#include <cstdint>
#include <optional>

namespace radiance::colour
{
    // 256 entries per channel, red then green then blue, 0..65535 - SetDeviceGammaRamp's layout.
    using Ramp = std::array<uint16_t, 768>;
    // Row-vector 5x5 colour matrix, row-major - the Magnification API's layout:
    // [r g b a 1] x M. Rows 0-2 hold the channel mix, row 4 the offsets.
    using Matrix = std::array<float, 25>;

    Ramp IdentityRamp();
    bool IsIdentity(const Ramp& ramp);
    Matrix IdentityMatrix();
    bool IsIdentity(const Matrix& matrix);
    Matrix Multiply(const Matrix& a, const Matrix& b);

    // How strongly each part of the tone curve moves things. PlexusX received these
    // from its server; these are Radiance's own values.
    struct GradeCurve
    {
        double regionRange = 0.22;   // highlights / shadows
        int regionSharpness = 2;
        double pointRange = 0.10;    // blacks / whites
        int pointSharpness = 4;
        double tintRange = 0.06;     // shadow / highlight tint
    };

    // Gamma, highlights, shadows, blacks, whites and the two tints.
    Ramp ToneRamp(const ColourLook& look, const GradeCurve& grade = {});

    // Brightness, contrast, temperature and RGB gains on top of a tone ramp.
    // `warmth` (0..1) and `dim` (0..0.8) are eye care.
    Ramp ComposeChannels(const Ramp& tone, const ColourLook& look, float warmth = 0.0f, float dim = 0.0f);

    // A single display's own gamma and warmth on top of the shared ramp.
    Ramp ComposeMonitor(const Ramp& base, int gammaPercent, int warmthPercent);

    // The whole per-channel stage for one display.
    Ramp DisplayRamp(const ColourLook& look, const EyeCare& eyeCare, int monitorGammaPercent, int monitorWarmthPercent);

    // Blends `wanted` toward identity; used when Windows refuses a strong ramp.
    Ramp Toward(const Ramp& wanted, double strength);

    // Vibrance up to 100 is the NVIDIA driver's own; past it Radiance multiplies in
    // software. Without the driver the whole range is software, 50 being neutral.
    float SoftwareVibranceFactor(int vibrance, bool driverAvailable);
    // The driver's 0..100 level for a vibrance slider value.
    int DriverLevel(int vibrance);

    // Saturation, software vibrance, hue and colour-vision correction.
    // nullopt when the look leaves colours alone.
    std::optional<Matrix> ColourMatrix(const ColourLook& look, bool driverAvailable);

    // Applies the filter matrix and then the ramp to 32-bit BGRA pixels, the order the
    // screen applies them in (for screenshots, which Windows takes before either).
    void ApplyToPixels(uint32_t* bgra, size_t count, const Ramp* ramp, const Matrix* matrix);
}
