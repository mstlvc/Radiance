// Radiance's widget kit, built the way the UI source builds its edited widgets:
// each control lays itself out at the cursor, registers an ImGui item, runs
// ButtonBehavior and draws itself, easing its colours with a little per-id state.
//
// Sizes passed in are logical (96 dpi) pixels; a width of 0 means "the rest of
// the row".
#pragma once

#include "core/Model.h"
#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace radiance::ui
{
    // ------------------------------------------------------------------ plumbing

    // ImLerp towards `target` at `speed` per second - the UI source's easing.
    float Ease(float current, float target, float speed = 6.0f);
    ImVec4 Ease(const ImVec4& current, const ImVec4& target, float speed = 6.0f);

    void Tip(const char* text);  // themed tooltip for the last item
    void Gap(float logical);     // vertical space

    // ------------------------------------------------------------------ containers

    // A translucent rounded panel (the UI source's child "Container"), scrolling on
    // its own with eased wheel scrolling.
    bool BeginPanel(const char* id, ImVec2 sizeDevicePx);
    void EndPanel();

    // ------------------------------------------------------------------ text rows

    void SectionTitle(const char* title, const std::string& aside = {});
    void Caption(std::string_view text, const ImVec4& colour = c::text::caption);
    void InfoRow(const char* name, std::string_view value, const ImVec4& valueColour = c::text::text);
    void Separator();
    // A boxed note in the style of the UI source's update comments.
    void Note(const char* title, std::string_view text, const char* tag = nullptr);

    // ------------------------------------------------------------------ controls

    // Label on the left, ON/OFF and a sliding switch on the right.
    bool Toggle(const char* label, bool* value, const char* tip = nullptr, bool enabled = true);

    enum class ButtonKind { Normal, Accent, Danger };
    bool Button(const char* label, ImVec2 size = ImVec2(0, 34), ButtonKind kind = ButtonKind::Normal, Icon icon = Icon::None, bool enabled = true);
    float ButtonWidth(const char* label, Icon icon = Icon::None);  // logical px

    // Icon-only control (title bar).
    bool IconButton(const char* id, Icon icon, ImVec2 sizeDevicePx, bool danger = false);
    // Sidebar page tile.
    bool PageTile(const char* id, Icon icon, bool selected, ImVec2 sizeDevicePx);

    // A selectable tile: outlined in the accent when chosen.
    bool Choice(const char* label, bool selected, ImVec2 size, bool enabled = true, const char* suffix = nullptr);
    // A row or grid of choices sharing the width. Returns true when the selection changed.
    bool ChoiceGrid(const char* id, int* index, const std::vector<std::string>& items, int columns, float height = 32, bool enabled = true);

    using Format = std::function<std::string(int)>;
    std::string Percent(int v);
    std::string Signed(int v);
    std::string Degrees(int v);
    std::string Gamma(int v);

    struct SliderSpec
    {
        const char* label;
        int minimum;
        int maximum;
        std::optional<int> notch;  // neutral tick, also where the fill starts
        Format format = Percent;
        const char* tip = nullptr;
        bool snap = true;          // wide ranges move in fives and pull to 100 / the notch
        std::optional<int> origin;   // where the fill starts, when that is not the notch
        std::optional<int> neutral;  // double-click target (and a snap point), when not the notch
    };
    bool Slider(const char* id, int* value, const SliderSpec& spec, bool enabled = true);
    // The big readout version (saturation, vibrance).
    bool HeroSlider(const char* id, int* value, const SliderSpec& spec, const char* hint, std::span<const char* const> ticks = {}, std::span<const float> tickFractions = {});

    bool Combo(const char* id, int* index, const std::vector<std::string>& items, const char* placeholder = "Choose\xE2\x80\xA6", float width = 0);

    // A text field with an icon square at its right end, as in the UI source.
    bool TextField(const char* id, char* buffer, size_t size, const char* hint, Icon icon = Icon::None, ImGuiInputTextFlags flags = 0,
        bool* submitted = nullptr, Type font = Type::Body, float width = 0);

    // Click, press a shortcut. Escape cancels, Backspace clears.
    bool HotkeyField(const char* id, Hotkey* hotkey, float width = 0);
    bool IsCapturingHotkey();

    // ------------------------------------------------------------------ cards

    struct CardSpec
    {
        std::string title;
        std::string tag;            // small text after the title (the source's version tag)
        std::string info;           // the line under the title
        bool selected = false;
        ImTextureID image = nullptr; // thumbnail on the right
        Icon icon = Icon::None;      // or an icon on the right, in the accent
        bool removable = false;      // shows a remove cross on hover
        float height = 84;
    };
    enum class CardResult { None, Clicked, Removed };
    // The UI source's "spoof panel": a selectable card.
    CardResult Card(const char* id, const CardSpec& spec);

    // ------------------------------------------------------------------ misc

    // The UI source's circular progress, centred at `centre`.
    void ProgressRing(ImDrawList* dl, ImVec2 centre, float radius, float progress, float thickness = 3.0f);

    void OpenDialog(const char* id);
    bool BeginDialog(const char* id, float logicalWidth);
    void CloseDialog();
    void EndDialog();
}
