// Radiance's built-in, read-only tables: the games it knows, the scene looks it
// ships with, the PvP resolution shortcuts and the colour-vision modes.
#pragma once

#include "core/Model.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace radiance::catalog
{
    std::span<const SupportedGame> Games();
    const SupportedGame* GameById(std::string_view id);

    // Games that have scene looks, in picker order.
    std::vector<std::string> LookGames();
    std::vector<LookPreset> LooksFor(std::string_view game);

    std::span<const PvpResolution> PvpResolutions();

    struct VisionMode
    {
        ColourVision mode;
        const char* label;
        const char* tip;
    };
    std::span<const VisionMode> VisionModes();

    struct ScalingChoice
    {
        ScalingMode mode;
        const char* label;
    };
    std::span<const ScalingChoice> ScalingChoices();

    // The Utilities page's tools.
    std::span<const Utility> Utilities();
}
