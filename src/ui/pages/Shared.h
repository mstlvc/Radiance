// Small pieces more than one page uses.
#pragma once

#include "core/Model.h"
#include "ui/Page.h"

#include "imgui.h"

#include <string>
#include <vector>

namespace radiance::ui
{
    // Width (logical px) of each of `count` equal controls sharing the current row.
    float Split(int count);

    // The two big sliders (Display page and the quick popup). True when either moved.
    bool SaturationAndVibrance(PageContext& ctx);

    // Picks one of Radiance's supported games; the choice is shared across pages.
    bool GameCombo(const char* id, std::string& gameId);

    std::string AspectLabel(int width, int height);  // "16:9", "21:9", "4:3"…
    std::string ModeText(const DisplayMode& mode);   // "2560 × 1440"
    std::string Megapixels(int width, int height);   // "3.7 MP"

    // "Saturation 130% · Vibrance 150% · Gamma 1.10", or "Changes nothing".
    std::string DescribeLook(const ColourLook& look);
}
