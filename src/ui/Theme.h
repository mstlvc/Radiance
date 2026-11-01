// Radiance's design tokens, in two themes, each dark or light:
//  - Radiance: the UI source's palette (imgui_settings.h) - a near-black blue-grey
//    ground, translucent panels and a periwinkle accent; its light version keeps the
//    shapes on a pale blue-grey ground with a deeper periwinkle;
//  - macOS: macOS 15 Sequoia, after imgui-apple's CupertinoUi palettes (MIT) - a flat
//    window, a sidebar, grouped sections and the system blue, dark or light.
// Everything is drawn through ImGui::GetColorU32(), so page fades (style alpha) apply to
// every pixel. SetTheme() swaps the values; whatever draws next uses them.
#pragma once

#include "imgui.h"

#include <string_view>

namespace radiance::ui
{
    enum class ThemeId
    {
        Radiance,
        MacOS,
    };

    // The values below are the Radiance dark theme's; SetTheme() replaces them.
    namespace c
    {
        inline ImVec4 accent = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 1.0f);

        inline ImVec4 green = ImVec4(49 / 255.f, 255 / 255.f, 57 / 255.f, 1.0f);
        inline ImVec4 orange = ImVec4(255 / 255.f, 173 / 255.f, 49 / 255.f, 1.0f);
        inline ImVec4 red = ImVec4(255 / 255.f, 49 / 255.f, 49 / 255.f, 1.0f);

        // The backdrop's particle network.
        inline ImVec4 particle = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 1.0f);

        namespace background
        {
            inline ImVec4 filling = ImVec4(24 / 255.f, 25 / 255.f, 29 / 255.f, 1.0f);
            inline ImVec4 stroke = ImVec4(31 / 255.f, 35 / 255.f, 49 / 255.f, 1.0f);
            inline float rounding = 8.0f;
        }

        namespace element
        {
            inline ImVec4 filling = ImVec4(26 / 255.f, 28 / 255.f, 35 / 255.f, 125 / 255.f);
            inline ImVec4 stroke = ImVec4(47 / 255.f, 53 / 255.f, 75 / 255.f, 80 / 255.f);
            inline ImVec4 panel = ImVec4(0.0f, 0.0f, 0.0f, 45 / 255.f);
            // Popups and tooltips sit over content, so they need a solid version of the fill.
            inline ImVec4 popup = ImVec4(22 / 255.f, 23 / 255.f, 28 / 255.f, 1.0f);
            inline float rounding = 8.0f;
        }

        namespace text
        {
            inline ImVec4 text_active = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            inline ImVec4 text_hov = ImVec4(92 / 255.f, 107 / 255.f, 161 / 255.f, 125 / 255.f);
            inline ImVec4 text = ImVec4(66 / 255.f, 72 / 255.f, 93 / 255.f, 1.0f);
            // Longer explanations: the same blue-grey, lifted enough to read as a paragraph.
            inline ImVec4 caption = ImVec4(104 / 255.f, 112 / 255.f, 140 / 255.f, 1.0f);
        }

        // Buttons, option chips, fields, switches and slider knobs.
        namespace control
        {
            inline ImVec4 rest = ImVec4(26 / 255.f, 28 / 255.f, 35 / 255.f, 125 / 255.f);
            inline ImVec4 hover = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 1.0f);
            inline ImVec4 accentRest = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 0.22f);
            inline ImVec4 accentHover = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 1.0f);
            inline ImVec4 accentStroke = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 0.45f);
            // Button text over each of the four fills above.
            inline ImVec4 text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            inline ImVec4 hoverText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            inline ImVec4 accentText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            inline ImVec4 accentHoverText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            inline ImVec4 knob = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 1.0f);
            inline ImVec4 switchKnob = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
            inline ImVec4 switchRim = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
            inline ImVec4 selected = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 0.10f);
            inline ImVec4 scrollbar = ImVec4(171 / 255.f, 189 / 255.f, 255 / 255.f, 0.40f);
            inline ImVec4 fieldFill = ImVec4(26 / 255.f, 28 / 255.f, 35 / 255.f, 125 / 255.f);
            inline ImVec4 fieldEdge = ImVec4(47 / 255.f, 53 / 255.f, 75 / 255.f, 80 / 255.f);
            inline float rounding = 2.0f;        // buttons
            inline float choiceRounding = 4.0f;  // option chips, notes, fields
        }

        // The macOS sidebar (the Radiance theme has none).
        namespace sidebar
        {
            inline ImVec4 filling = ImVec4(53 / 255.f, 53 / 255.f, 53 / 255.f, 1.0f);
            inline ImVec4 divider = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            inline ImVec4 selection = ImVec4(0.0f, 88 / 255.f, 208 / 255.f, 1.0f);
            inline ImVec4 text = ImVec4(1.0f, 1.0f, 1.0f, 0.92f);
            inline ImVec4 selectedText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            inline ImVec4 hover = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
        }
    }

    ThemeId Theme();
    bool Light();
    // The macOS theme, whose shell and a few controls are drawn differently.
    inline bool Mac() { return Theme() == ThemeId::MacOS; }
    // Swaps every token for the theme's. Call ApplyStyle() afterwards (once ImGui is up)
    // and rebuild the fonts: each theme has its own typeface.
    void SetTheme(ThemeId theme, bool light);
    // "radiance" / "macos", as the settings file stores it.
    ThemeId ThemeFromName(std::string_view name);
    const char* ThemeName(ThemeId theme);

    // Packs a palette colour with the current style alpha (and an optional extra alpha).
    inline ImU32 Col(const ImVec4& colour, float alpha = 1.0f) { return ImGui::GetColorU32(colour, alpha); }
    ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t);

    // Logical (96 dpi) pixels -> device pixels for the window's current monitor.
    float Scale();
    void SetScale(float scale);
    inline float S(float logical) { return logical * Scale(); }
    inline ImVec2 S(float x, float y) { return ImVec2(x * Scale(), y * Scale()); }

    // Pushes the palette into ImGui's style (scrollbars, popups, text fields) and the
    // core-widget hooks in imgui_radiance.h. Call after every scale or theme change.
    void ApplyStyle();
}
