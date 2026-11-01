// Radiance's type: Lexend, as in the UI source (embedded in the exe), loaded the
// same way (FreeType, auto-hinted, nominal pixel sizes). The macOS theme uses
// Windows' Segoe UI in the same sizes. The size names below are Lexend's.
#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <string_view>

namespace radiance::ui
{
    enum class Type
    {
        Small,        // Lexend Regular 12 - captions, hints
        SmallMedium,  // Lexend Medium 12 - pills, small labels
        Body,         // Lexend Medium 14 - the UI source's default
        BodyRegular,  // Lexend Regular 14 - values, secondary rows
        Button,       // Lexend Medium 13 - buttons (the source's lexend_medium_x)
        Heading,      // Lexend Medium 16 - panel section titles
        Title,        // Lexend Medium 19 - page title in the top bar
        Value,        // Lexend Medium 30 - big readouts
        Brand,        // Lexend Bold 17 - the wordmark
        Mono,         // Cascadia Mono / Consolas 13 - codes and hashes
        Count
    };

    // Builds the atlas for the current ui::Scale(). Returns false when the embedded
    // fonts could not be loaded (ImGui's default font is used then).
    bool BuildFonts(ImFontAtlas& atlas);

    ImFont* Font(Type type);
    ImVec2 Measure(Type type, std::string_view text, float wrapWidth = 0.0f);
    // Height of the glyph box (ascent + descent), for centring text in a row.
    inline float TextHeight(Type type) { ImFont* f = Font(type); return f->Ascent - f->Descent; }

    void PaintText(ImDrawList* dl, Type type, ImVec2 at, ImU32 colour, std::string_view text);
    // Word-wrapped; returns the height used.
    float PaintWrapped(ImDrawList* dl, Type type, ImVec2 at, float width, ImU32 colour, std::string_view text);
    // One line inside `box`, vertically centred, with an ellipsis when it doesn't fit.
    void PaintClipped(ImDrawList* dl, Type type, const ImRect& box, ImU32 colour, std::string_view text, float alignX = 0.0f);

    class FontScope
    {
    public:
        explicit FontScope(Type type) { ImGui::PushFont(Font(type)); }
        ~FontScope() { ImGui::PopFont(); }
        FontScope(const FontScope&) = delete;
        FontScope& operator=(const FontScope&) = delete;
    };
}
