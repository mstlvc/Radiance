// Radiance theme hooks for the few Dear ImGui core widgets Radiance restyles in
// place (the single-line text field and the scrollbar render in imgui_widgets.cpp).
// The values are written by radiance::ui::Theme whenever the palette is applied,
// so the core never needs to know about Radiance's own headers.
#pragma once
#include "imgui.h"

namespace ImRadiance
{
    inline ImU32 FieldFill      = IM_COL32(0, 0, 0, 0);
    inline ImU32 FieldEdge      = IM_COL32(116, 116, 116, 255);
    inline ImU32 FieldEdgeHot   = IM_COL32(193, 193, 193, 255);
    inline ImU32 FieldEdgeFocus = IM_COL32(255, 255, 255, 255);
    inline ImU32 Text           = IM_COL32(243, 243, 243, 255);
    inline ImU32 TextHint       = IM_COL32(153, 153, 153, 255);
    inline ImU32 Selection      = IM_COL32(255, 255, 255, 56);
    inline ImU32 Caret          = IM_COL32(255, 255, 255, 255);
    inline float FieldRounding  = 4.0f;
    inline float FieldBorder    = 1.0f;
    // Width kept free at a text field's right end for its icon (set per field).
    inline float FieldIconReserve = 0.0f;
}
