#include "ui/Theme.h"

#include "imgui_radiance.h"

#include <algorithm>
#include <cmath>

namespace radiance::ui
{
    namespace
    {
        float g_scale = 1.0f;
        ThemeId g_theme = ThemeId::Radiance;
        bool g_light = false;

        constexpr ImVec4 Rgb(int r, int g, int b, float a = 1.0f)
        {
            return ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
        }

        constexpr ImVec4 White(float a) { return ImVec4(1.0f, 1.0f, 1.0f, a); }
        constexpr ImVec4 Black(float a) { return ImVec4(0.0f, 0.0f, 0.0f, a); }

        constexpr ImVec4 WithAlpha(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); }

        // What the Radiance palettes share: the UI source's shapes, the accent doing the
        // work of selection, buttons that fill with it on hover.
        void RadianceShapes()
        {
            c::control::accentStroke = WithAlpha(c::accent, 0.45f);
            c::control::hover = c::accent;
            c::control::accentHover = c::accent;
            c::control::knob = c::accent;
            c::control::rounding = 2.0f;
            c::control::choiceRounding = 4.0f;
            c::background::rounding = 8.0f;
            c::element::rounding = 8.0f;
            c::particle = c::accent;
        }

        // The UI source's spoofer palette.
        void UseRadianceDark()
        {
            c::accent = Rgb(171, 189, 255);
            c::green = Rgb(49, 255, 57);
            c::orange = Rgb(255, 173, 49);
            c::red = Rgb(255, 49, 49);

            c::background::filling = Rgb(24, 25, 29);
            c::background::stroke = Rgb(31, 35, 49);

            c::element::filling = Rgb(26, 28, 35, 125 / 255.f);
            c::element::stroke = Rgb(47, 53, 75, 80 / 255.f);
            c::element::panel = Black(45 / 255.f);
            c::element::popup = Rgb(22, 23, 28);

            c::text::text_active = White(1.0f);
            c::text::text_hov = Rgb(92, 107, 161, 125 / 255.f);
            c::text::text = Rgb(66, 72, 93);
            c::text::caption = Rgb(104, 112, 140);

            c::control::rest = c::element::filling;
            c::control::accentRest = WithAlpha(c::accent, 0.22f);
            c::control::text = White(1.0f);
            c::control::hoverText = White(1.0f);
            c::control::accentText = White(1.0f);
            c::control::accentHoverText = White(1.0f);
            c::control::selected = WithAlpha(c::accent, 0.10f);
            c::control::scrollbar = WithAlpha(c::accent, 0.40f);
            c::control::fieldFill = c::element::filling;
            c::control::fieldEdge = c::element::stroke;
            RadianceShapes();
        }

        // The same design in daylight: a pale blue-grey ground, white panels, navy text
        // and a deeper periwinkle so the accent still reads.
        void UseRadianceLight()
        {
            c::accent = Rgb(88, 104, 214);
            c::green = Rgb(22, 150, 60);
            c::orange = Rgb(205, 116, 0);
            c::red = Rgb(214, 40, 40);

            c::background::filling = Rgb(236, 238, 245);
            c::background::stroke = Rgb(200, 205, 222);

            c::element::filling = White(0.70f);
            c::element::stroke = Rgb(176, 184, 212, 0.75f);
            c::element::panel = White(0.55f);
            c::element::popup = Rgb(250, 251, 254);

            c::text::text_active = Rgb(24, 27, 40);
            c::text::text_hov = Rgb(110, 120, 160, 0.85f);
            c::text::text = Rgb(112, 120, 146);
            c::text::caption = Rgb(88, 96, 122);

            c::control::rest = c::element::filling;
            c::control::accentRest = WithAlpha(c::accent, 0.16f);
            c::control::text = c::text::text_active;
            c::control::hoverText = White(1.0f);  // white on the full accent
            c::control::accentText = c::text::text_active;
            c::control::accentHoverText = White(1.0f);
            c::control::selected = WithAlpha(c::accent, 0.12f);
            c::control::scrollbar = WithAlpha(c::accent, 0.45f);
            c::control::fieldFill = White(0.85f);
            c::control::fieldEdge = c::element::stroke;
            RadianceShapes();
        }

        // What the macOS palettes share.
        void MacShapes()
        {
            c::background::rounding = 10.0f;
            c::element::rounding = 10.0f;
            c::control::rounding = 6.0f;
            c::control::choiceRounding = 6.0f;
            c::control::accentText = White(1.0f);
            c::control::accentHoverText = White(1.0f);
            c::sidebar::selectedText = White(1.0f);
        }

        // macOS 15 Sequoia, dark, with the blue accent: imgui-apple's MacDark palette
        // (window #2D2D2D, sidebar #353535, labels at 85% white, sections at 2% with a
        // rim, controls at 25%, knobs #C9C9C9).
        void UseMacDark()
        {
            c::accent = Rgb(10, 132, 255);
            c::green = Rgb(50, 215, 75);
            c::orange = Rgb(255, 159, 10);
            c::red = Rgb(255, 69, 58);
            c::particle = White(1.0f);

            c::background::filling = Rgb(45, 45, 45);
            c::background::stroke = White(0.20f);

            c::element::filling = White(0.07f);
            c::element::stroke = White(0.10f);
            c::element::panel = White(0.03f);
            c::element::popup = Rgb(43, 43, 43);

            c::text::text_active = White(0.88f);
            c::text::text_hov = White(0.40f);
            c::text::text = White(0.55f);
            c::text::caption = White(0.55f);

            c::control::rest = White(0.20f);
            c::control::hover = White(0.27f);
            c::control::accentRest = Rgb(16, 118, 236);
            c::control::accentHover = Rgb(48, 140, 255);
            c::control::accentStroke = White(0.12f);
            c::control::text = White(0.88f);
            c::control::hoverText = White(0.95f);
            c::control::knob = Rgb(201, 201, 201);
            c::control::switchKnob = Rgb(242, 242, 242);
            c::control::switchRim = White(0.12f);
            c::control::selected = White(0.22f);
            c::control::scrollbar = White(0.35f);
            c::control::fieldFill = Rgb(30, 30, 30);
            c::control::fieldEdge = White(0.12f);

            c::sidebar::filling = Rgb(53, 53, 53);
            c::sidebar::divider = Black(1.0f);
            c::sidebar::selection = Rgb(0, 88, 208);
            c::sidebar::text = White(0.92f);
            c::sidebar::hover = White(0.06f);
            MacShapes();
        }

        // macOS 15 Sequoia, light: imgui-apple's MacLight palette (window #F6F6F6,
        // sidebar #E9E9E9, labels at 85% black, white controls and knobs with faint rims).
        void UseMacLight()
        {
            c::accent = Rgb(0, 122, 255);
            c::green = Rgb(40, 180, 65);
            c::orange = Rgb(230, 134, 0);
            c::red = Rgb(255, 59, 48);
            c::particle = Black(1.0f);

            c::background::filling = Rgb(246, 246, 246);
            c::background::stroke = Black(0.13f);

            c::element::filling = Black(0.05f);
            c::element::stroke = Black(0.09f);
            c::element::panel = Black(0.025f);
            c::element::popup = Rgb(252, 252, 252);

            c::text::text_active = Black(0.85f);
            c::text::text_hov = Black(0.35f);
            c::text::text = Black(0.50f);
            c::text::caption = Black(0.55f);

            c::control::rest = White(1.0f);
            c::control::hover = Rgb(242, 242, 242);
            c::control::accentRest = Rgb(0, 122, 255);
            c::control::accentHover = Rgb(26, 136, 255);
            c::control::accentStroke = Black(0.08f);
            c::control::text = Black(0.85f);
            c::control::hoverText = Black(0.85f);
            c::control::knob = White(1.0f);
            c::control::switchKnob = White(1.0f);
            c::control::switchRim = Black(0.08f);
            c::control::selected = White(1.0f);
            c::control::scrollbar = Black(0.35f);
            c::control::fieldFill = White(1.0f);
            c::control::fieldEdge = Black(0.10f);

            c::sidebar::filling = Rgb(233, 233, 233);
            c::sidebar::divider = Black(0.10f);
            c::sidebar::selection = Rgb(0, 100, 225);
            c::sidebar::text = Black(0.85f);
            c::sidebar::hover = Black(0.05f);
            MacShapes();
        }
    }

    ThemeId Theme()
    {
        return g_theme;
    }

    bool Light()
    {
        return g_light;
    }

    void SetTheme(ThemeId theme, bool light)
    {
        g_theme = theme;
        g_light = light;
        if (theme == ThemeId::MacOS)
            light ? UseMacLight() : UseMacDark();
        else
            light ? UseRadianceLight() : UseRadianceDark();
    }

    ThemeId ThemeFromName(std::string_view name)
    {
        return name == "macos" ? ThemeId::MacOS : ThemeId::Radiance;
    }

    const char* ThemeName(ThemeId theme)
    {
        return theme == ThemeId::MacOS ? "macos" : "radiance";
    }

    ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }

    float Scale()
    {
        return g_scale;
    }

    void SetScale(float scale)
    {
        g_scale = std::clamp(scale, 0.5f, 4.0f);
    }

    void ApplyStyle()
    {
        ImGuiStyle style;
        if (Light())
            ImGui::StyleColorsLight(&style);
        else
            ImGui::StyleColorsDark(&style);

        style.WindowPadding = ImVec2(0, 0);
        style.WindowBorderSize = 0;
        style.WindowRounding = 0;
        style.ChildBorderSize = 0;
        style.ChildRounding = S(c::element::rounding);
        style.FramePadding = S(10, 6);
        style.FrameRounding = S(c::control::choiceRounding);
        style.FrameBorderSize = 0;
        style.ItemSpacing = S(10, 12);
        style.ItemInnerSpacing = S(6, 6);
        style.PopupRounding = S(Mac() ? 8.0f : 6.0f);
        style.PopupBorderSize = 1.0f;
        style.ScrollbarSize = S(7);  // as in the UI source
        style.ScrollbarRounding = S(30);
        style.GrabRounding = S(30);
        style.AntiAliasedLines = true;
        style.AntiAliasedFill = true;
        style.HoverDelayNormal = 0.45f;

        const ImVec4& grab = c::control::scrollbar;
        ImVec4* col = style.Colors;
        col[ImGuiCol_Text] = c::text::text_active;
        col[ImGuiCol_TextDisabled] = c::text::text;
        col[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
        col[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        col[ImGuiCol_PopupBg] = c::element::popup;
        col[ImGuiCol_Border] = c::element::stroke;
        col[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        col[ImGuiCol_FrameBg] = c::element::filling;
        col[ImGuiCol_FrameBgHovered] = c::element::filling;
        col[ImGuiCol_FrameBgActive] = c::element::filling;
        col[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        col[ImGuiCol_ScrollbarGrab] = grab;
        col[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(grab, std::min(1.0f, grab.w * 1.5f));
        col[ImGuiCol_ScrollbarGrabActive] = WithAlpha(grab, std::min(1.0f, grab.w * 2.0f));
        col[ImGuiCol_TextSelectedBg] = WithAlpha(c::accent, Mac() || Light() ? 0.30f : 0.10f);
        col[ImGuiCol_NavHighlight] = c::accent;
        col[ImGuiCol_ModalWindowDimBg] = Light() ? Black(0.20f) : (Mac() ? Black(0.38f) : ImVec4(0.03f, 0.03f, 0.05f, 0.65f));
        ImGui::GetStyle() = style;

        ImRadiance::FieldFill = ImGui::ColorConvertFloat4ToU32(c::control::fieldFill);
        ImRadiance::FieldEdge = ImGui::ColorConvertFloat4ToU32(c::control::fieldEdge);
        ImRadiance::FieldEdgeHot = ImGui::ColorConvertFloat4ToU32(WithAlpha(c::accent, 0.35f));
        ImRadiance::FieldEdgeFocus = ImGui::ColorConvertFloat4ToU32(WithAlpha(c::accent, 0.70f));
        ImRadiance::Text = ImGui::ColorConvertFloat4ToU32(c::text::text_active);
        ImRadiance::TextHint = ImGui::ColorConvertFloat4ToU32(c::text::text);
        ImRadiance::Selection = ImGui::ColorConvertFloat4ToU32(WithAlpha(c::accent, Mac() || Light() ? 0.30f : 0.10f));
        ImRadiance::Caret = ImGui::ColorConvertFloat4ToU32(c::text::text_active);
        ImRadiance::FieldRounding = S(c::control::choiceRounding);
        ImRadiance::FieldBorder = 1.0f;
    }
}
