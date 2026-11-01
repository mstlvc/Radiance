#include "ui/Widgets.h"

#include "imgui_radiance.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // virtual-key codes for the hotkey field

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace radiance::ui
{
    using namespace ImGui;

    namespace
    {
        constexpr float kWheelStep = 66;       // px per wheel notch
        constexpr float kScrollResponse = 16;  // how quickly a panel glides to its scroll target

        // Per-id animation state, kept the way the UI source keeps it.
        template <typename T>
        T& StateFor(ImGuiID id, const T& initial)
        {
            static std::unordered_map<ImGuiID, T> states;
            return states.try_emplace(id, initial).first->second;
        }

        float RowWidth(float logicalWidth)
        {
            return logicalWidth > 0 ? std::round(S(logicalWidth)) : std::floor(GetContentRegionAvail().x);
        }

        // Lays an item of `size` at the cursor and registers it. Returns false when it
        // is scrolled out of view (nothing to draw or click then).
        bool Place(ImGuiID id, ImVec2 size, ImRect& bb, bool enabled = true)
        {
            ImGuiWindow* window = GetCurrentWindow();
            const ImVec2 pos = window->DC.CursorPos;
            bb = ImRect(pos, pos + ImVec2(std::round(size.x), std::round(size.y)));
            ItemSize(bb, 0.0f);
            if (!enabled)
                PushItemFlag(ImGuiItemFlags_Disabled, true);
            const bool added = ItemAdd(bb, id);
            if (!enabled)
                PopItemFlag();
            return added;
        }

        bool KeyboardFocused(ImGuiID id)
        {
            const ImGuiContext& g = *GImGui;
            return g.NavId == id && !g.NavDisableHighlight && g.NavWindow == g.CurrentWindow;
        }

        void FocusRing(const ImRect& bb, float rounding)
        {
            const float gap = S(3);
            GetWindowDrawList()->AddRect(bb.Min - ImVec2(gap, gap), bb.Max + ImVec2(gap, gap), Col(c::accent, 0.8f), rounding + gap, 0, S(1.5f));
        }

        ImVec4 Fade(const ImVec4& colour, float alpha)
        {
            return ImVec4(colour.x, colour.y, colour.z, colour.w * alpha);
        }

        // Wide ranges move in fives, and 100 / the notch / neutral pull the knob in from
        // three away so they are easy to hit.
        int Land(int raw, int minimum, int maximum, std::optional<int> notch, std::optional<int> neutral)
        {
            raw = std::clamp(raw, minimum, maximum);
            if (maximum - minimum < 80)
                return raw;
            constexpr int kStep = 5;
            constexpr int kPull = 3;
            std::optional<int> best;
            for (int target : { 100, notch.value_or(100), neutral.value_or(100) })
            {
                if (target < minimum || target > maximum || std::abs(raw - target) > kPull)
                    continue;
                if (!best || std::abs(raw - target) < std::abs(raw - *best))
                    best = target;
            }
            if (best)
                return *best;
            const int below = static_cast<int>(std::floor(static_cast<double>(raw) / kStep)) * kStep;
            int stepped = (raw - below <= below + kStep - raw) ? below : below + kStep;
            stepped = std::clamp(stepped, minimum, maximum);
            const int drift = std::abs(raw - stepped);
            if (raw - minimum < drift)
                return minimum;
            if (maximum - raw < drift)
                return maximum;
            return stepped;
        }

        uint32_t VkFromImGuiKey(ImGuiKey key)
        {
            if (key >= ImGuiKey_A && key <= ImGuiKey_Z) return 'A' + (key - ImGuiKey_A);
            if (key >= ImGuiKey_0 && key <= ImGuiKey_9) return '0' + (key - ImGuiKey_0);
            if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) return VK_F1 + (key - ImGuiKey_F1);
            if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) return VK_NUMPAD0 + (key - ImGuiKey_Keypad0);
            switch (key)
            {
            case ImGuiKey_Space: return VK_SPACE;
            case ImGuiKey_Tab: return VK_TAB;
            case ImGuiKey_Enter: return VK_RETURN;
            case ImGuiKey_Insert: return VK_INSERT;
            case ImGuiKey_Delete: return VK_DELETE;
            case ImGuiKey_Home: return VK_HOME;
            case ImGuiKey_End: return VK_END;
            case ImGuiKey_PageUp: return VK_PRIOR;
            case ImGuiKey_PageDown: return VK_NEXT;
            case ImGuiKey_LeftArrow: return VK_LEFT;
            case ImGuiKey_RightArrow: return VK_RIGHT;
            case ImGuiKey_UpArrow: return VK_UP;
            case ImGuiKey_DownArrow: return VK_DOWN;
            case ImGuiKey_Apostrophe: return VK_OEM_7;
            case ImGuiKey_Comma: return VK_OEM_COMMA;
            case ImGuiKey_Minus: return VK_OEM_MINUS;
            case ImGuiKey_Period: return VK_OEM_PERIOD;
            case ImGuiKey_Slash: return VK_OEM_2;
            case ImGuiKey_Semicolon: return VK_OEM_1;
            case ImGuiKey_Equal: return VK_OEM_PLUS;
            case ImGuiKey_LeftBracket: return VK_OEM_4;
            case ImGuiKey_Backslash: return VK_OEM_5;
            case ImGuiKey_RightBracket: return VK_OEM_6;
            case ImGuiKey_GraveAccent: return VK_OEM_3;
            case ImGuiKey_Pause: return VK_PAUSE;
            case ImGuiKey_PrintScreen: return VK_SNAPSHOT;
            case ImGuiKey_ScrollLock: return VK_SCROLL;
            case ImGuiKey_KeypadDecimal: return VK_DECIMAL;
            case ImGuiKey_KeypadDivide: return VK_DIVIDE;
            case ImGuiKey_KeypadMultiply: return VK_MULTIPLY;
            case ImGuiKey_KeypadSubtract: return VK_SUBTRACT;
            case ImGuiKey_KeypadAdd: return VK_ADD;
            default: return 0;
            }
        }

        ImGuiID g_capturing = 0;
    }

    // ------------------------------------------------------------------ plumbing

    float Ease(float current, float target, float speed)
    {
        return ImLerp(current, target, ImSaturate(GetIO().DeltaTime * speed));
    }

    ImVec4 Ease(const ImVec4& current, const ImVec4& target, float speed)
    {
        return ImLerp(current, target, ImSaturate(GetIO().DeltaTime * speed));
    }

    void Tip(const char* text)
    {
        if (!text || !*text)
            return;
        if (!IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_AllowWhenDisabled))
            return;
        PushStyleVar(ImGuiStyleVar_WindowPadding, S(12, 9));
        PushStyleVar(ImGuiStyleVar_PopupRounding, S(6));
        PushStyleColor(ImGuiCol_PopupBg, c::element::popup);
        PushStyleColor(ImGuiCol_Border, c::element::stroke);
        if (BeginTooltip())
        {
            // The font has to be popped inside the tooltip: PopFont() also pops the font
            // texture off the *current* window's draw list, and after EndTooltip() that is
            // the window underneath, whose later items would then draw with no texture.
            {
                FontScope font(Type::Small);
                PushStyleColor(ImGuiCol_Text, c::text::text_active);
                PushTextWrapPos(S(300));
                TextUnformatted(text);
                PopTextWrapPos();
                PopStyleColor();
            }
            EndTooltip();
        }
        PopStyleColor(2);
        PopStyleVar(2);
    }

    void Gap(float logical)
    {
        SetCursorPosY(GetCursorPosY() + S(logical));
    }

    // ------------------------------------------------------------------ containers

    namespace
    {
        struct PanelScroll
        {
            float target = 0.0f;
            float applied = 0.0f;
        };
    }

    bool BeginPanel(const char* id, ImVec2 size)
    {
        ImDrawList* parent = GetWindowDrawList();
        const ImVec2 pos = GetCursorScreenPos();
        parent->AddRectFilled(pos, pos + size, Col(c::element::panel), S(c::element::rounding));
        parent->AddRect(pos, pos + size, Col(c::element::stroke), S(c::element::rounding));

        PushStyleVar(ImGuiStyleVar_WindowPadding, S(18, 18));
        PushStyleVar(ImGuiStyleVar_ItemSpacing, S(10, 14));
        const bool open = BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NavFlattened);

        // Eased wheel scrolling: the wheel moves a target and the panel glides to it.
        ImGuiWindow* window = GetCurrentWindow();
        PanelScroll& scroll = StateFor(window->ID, PanelScroll{});
        const ImGuiIO& io = GetIO();
        if (std::abs(window->Scroll.y - scroll.applied) > 1.0f)
            scroll.target = window->Scroll.y;  // the scrollbar or keyboard moved it
        if (io.MouseWheel != 0.0f && IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !IsAnyItemActive())
            scroll.target -= io.MouseWheel * S(kWheelStep);
        scroll.target = std::round(std::clamp(scroll.target, 0.0f, GetScrollMaxY()));
        const float delta = scroll.target - window->Scroll.y;
        float next = window->Scroll.y;
        if (std::abs(delta) >= 1.0f)
        {
            float step = delta * std::min(1.0f, io.DeltaTime * kScrollResponse);
            if (std::abs(step) < 1.0f)
                step = delta > 0 ? 1.0f : -1.0f;
            next = std::round(window->Scroll.y + step);
            SetScrollY(next);
        }
        scroll.applied = next;
        return open;
    }

    void EndPanel()
    {
        Dummy(ImVec2(1, S(2)));  // keep the last item off the bottom edge when scrolled to the end
        EndChild();
        PopStyleVar(2);
    }

    // ------------------------------------------------------------------ text rows

    void SectionTitle(const char* title, const std::string& aside)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;
        const float w = RowWidth(0);
        const float h = TextHeight(Type::Heading);
        ImRect bb;
        if (!Place(window->GetID(title), ImVec2(w, h), bb))
            return;
        ImDrawList* dl = GetWindowDrawList();
        PaintText(dl, Type::Heading, bb.Min, Col(c::text::text_active), title);
        if (!aside.empty())
        {
            const float aw = Measure(Type::Small, aside).x;
            PaintClipped(dl, Type::Small, ImRect(ImVec2(bb.Max.x - aw, bb.Min.y), bb.Max), Col(c::text::text), aside, 1.0f);
        }
    }

    void Caption(std::string_view text, const ImVec4& colour)
    {
        if (text.empty())
            return;
        const float w = RowWidth(0);
        const ImVec2 size = Measure(Type::Small, text, w);
        ImRect bb;
        if (!Place(GetCurrentWindow()->GetID(text.data(), text.data() + text.size()), ImVec2(w, size.y), bb))
            return;
        PaintWrapped(GetWindowDrawList(), Type::Small, bb.Min, w, Col(colour), text);
    }

    void InfoRow(const char* name, std::string_view value, const ImVec4& valueColour)
    {
        // The UI source's info_bar: a white name on the left, a quiet value on the right.
        const float w = RowWidth(0);
        const float h = TextHeight(Type::Body);
        ImRect bb;
        if (!Place(GetCurrentWindow()->GetID(name), ImVec2(w, h), bb))
            return;
        ImDrawList* dl = GetWindowDrawList();
        const float nameW = Measure(Type::Body, name).x;
        PaintClipped(dl, Type::Body, ImRect(bb.Min, ImVec2(bb.Min.x + nameW + 1, bb.Max.y)), Col(c::text::text_active), name);
        PaintClipped(dl, Type::BodyRegular, ImRect(ImVec2(bb.Min.x + nameW + S(16), bb.Min.y), bb.Max), Col(valueColour), value, 1.0f);
    }

    void Separator()
    {
        // The UI source's Separator_line.
        const float w = RowWidth(0);
        ImRect bb;
        Place(0, ImVec2(w, 1), bb);
        GetWindowDrawList()->AddRectFilled(bb.Min, ImVec2(bb.Max.x, bb.Min.y + 1), Col(c::element::stroke));
    }

    void Note(const char* title, std::string_view text, const char* tag)
    {
        // The UI source's "comments" box.
        const float w = RowWidth(0);
        const float pad = S(12);
        const float titleH = TextHeight(Type::Body);
        const float textH = Measure(Type::Button, text, w - 2 * pad).y;
        ImRect bb;
        if (!Place(GetCurrentWindow()->GetID(title), ImVec2(w, pad + titleH + S(8) + textH + pad), bb))
            return;
        ImDrawList* dl = GetWindowDrawList();
        dl->AddRectFilled(bb.Min, bb.Max, Col(c::element::filling), S(c::control::choiceRounding));
        dl->AddRect(bb.Min, bb.Max, Col(c::element::stroke), S(c::control::choiceRounding));
        PaintText(dl, Type::Body, bb.Min + ImVec2(pad, pad), Col(c::accent), title);
        if (tag && *tag)
            PaintText(dl, Type::Body, bb.Min + ImVec2(pad + Measure(Type::Body, title).x + S(6), pad), Col(c::text::text_hov), tag);
        PaintWrapped(dl, Type::Button, bb.Min + ImVec2(pad, pad + titleH + S(8)), w - 2 * pad, Col(c::text::text_active), text);
    }

    // ------------------------------------------------------------------ controls

    bool Toggle(const char* label, bool* value, const char* tip, bool enabled)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID id = window->GetID(label);
        const float w = RowWidth(0);
        const float h = S(20);
        ImRect bb;
        if (!Place(id, ImVec2(w, h), bb, enabled))
            return false;
        bool hovered = false, held = false;
        const bool pressed = ButtonBehavior(bb, id, &hovered, &held);
        if (pressed && value)
        {
            *value = !*value;
            MarkItemEdited(id);
        }
        const bool on = value && *value;

        struct State { ImVec4 background, circle, text; float opacity, offset; };
        const State initial{ on ? c::accent : c::element::filling, on ? c::accent : c::text::text, on ? c::text::text_active : c::text::text, on ? 0.2f : 1.0f, on ? 0.0f : -S(40) };
        State& s = StateFor(id, initial);
        s.opacity = Ease(s.opacity, on ? 0.2f : 1.0f);
        s.offset = Ease(s.offset, on ? 0.0f : -S(40));
        s.background = Ease(s.background, on ? c::accent : c::element::filling);
        s.circle = Ease(s.circle, on ? c::accent : c::text::text);
        s.text = Ease(s.text, (on || hovered) && enabled ? c::text::text_active : c::text::text);

        ImDrawList* dl = GetWindowDrawList();
        const std::string_view text(label, FindRenderedTextEnd(label) - label);
        if (Mac())
        {
            // A macOS switch: grey off, the accent on, a light knob that slides across.
            float& slide = StateFor(id + 1, on ? 1.0f : 0.0f);
            slide = Ease(slide, on ? 1.0f : 0.0f, 10.0f);
            const ImRect track(ImVec2(bb.Max.x - S(36), bb.GetCenter().y - S(10)), ImVec2(bb.Max.x, bb.GetCenter().y + S(10)));
            dl->AddRectFilled(track.Min, track.Max, Col(Lerp(c::element::stroke, c::accent, slide)), S(10));
            dl->AddRect(track.Min, track.Max, Col(c::control::switchRim, 1.0f - slide), S(10));
            const float knobX = track.Min.x + S(10) + (track.GetWidth() - S(20)) * slide;
            dl->AddCircleFilled(ImVec2(knobX, track.GetCenter().y + S(0.5f)), S(9), Col(ImVec4(0, 0, 0, 0.25f)), 0);
            dl->AddCircleFilled(ImVec2(knobX, track.GetCenter().y), S(8.5f), Col(enabled ? c::control::switchKnob : c::text::text), 0);
            const float textY = bb.Min.y + (h - TextHeight(Type::Body)) * 0.5f;
            PaintClipped(dl, Type::BodyRegular, ImRect(ImVec2(bb.Min.x, textY), ImVec2(track.Min.x - S(12), textY + TextHeight(Type::Body))),
                Col(enabled ? c::text::text_active : c::text::text), text);
            if (KeyboardFocused(id))
                FocusRing(track, S(10));
            Tip(tip);
            return pressed;
        }
        const ImRect track(ImVec2(bb.Max.x - S(40), bb.Min.y), bb.Max);
        dl->AddRectFilled(track.Min, track.Max, Col(s.background, s.opacity), S(30));
        dl->AddRect(track.Min, track.Max, Col(c::element::stroke), S(30));
        dl->AddCircleFilled(ImVec2(track.Max.x - (h - s.offset) * 0.5f, track.GetCenter().y), S(5), Col(s.circle), 30);

        const char* state = on ? "ON" : "OFF";
        const float stateW = Measure(Type::Body, state).x;
        const float textY = bb.Min.y + (h - TextHeight(Type::Body)) * 0.5f;
        PaintText(dl, Type::Body, ImVec2(bb.Max.x - S(55) - stateW, textY), Col(s.text), state);
        PaintClipped(dl, Type::Body, ImRect(bb.Min, ImVec2(bb.Max.x - S(70) - stateW, bb.Max.y)), Col(s.text), text);
        if (KeyboardFocused(id))
            FocusRing(track, S(30));
        Tip(tip);
        return pressed;
    }

    float ButtonWidth(const char* label, Icon icon)
    {
        const char* end = FindRenderedTextEnd(label);
        const float text = end > label ? Measure(Type::Button, std::string_view(label, end - label)).x : 0.0f;
        float w = text + S(28);
        if (icon != Icon::None)
            w += S(16) + (end > label ? S(8) : 0.0f);
        return std::ceil(w / Scale());
    }

    bool Button(const char* label, ImVec2 size, ButtonKind kind, Icon icon, bool enabled)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID id = window->GetID(label);
        ImRect bb;
        if (!Place(id, ImVec2(RowWidth(size.x), std::round(S(size.y > 0 ? size.y : 34))), bb, enabled))
            return false;
        bool hovered = false, held = false;
        const bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        const ImVec4 hot = kind == ButtonKind::Danger ? c::red : (kind == ButtonKind::Accent ? c::control::accentHover : c::control::hover);
        const ImVec4 rest = kind == ButtonKind::Accent ? c::control::accentRest : c::control::rest;
        ImVec4& background = StateFor(id, rest);
        background = Ease(background, enabled && (hovered || held) ? hot : rest);

        ImDrawList* dl = GetWindowDrawList();
        const float rounding = S(c::control::rounding);
        dl->AddRectFilled(bb.Min, bb.Max, Col(background), rounding);
        dl->AddRect(bb.Min, bb.Max, Col(kind == ButtonKind::Accent ? c::control::accentStroke : c::element::stroke), rounding);

        const ImVec4 restInk = kind == ButtonKind::Accent ? c::control::accentText : c::control::text;
        const ImVec4 hotInk = kind == ButtonKind::Normal ? c::control::hoverText : c::control::accentHoverText;
        ImVec4& inkState = StateFor(id + 1, restInk);
        inkState = Ease(inkState, enabled && (hovered || held) ? hotInk : restInk);
        const ImU32 ink = Col(enabled ? inkState : c::text::text);
        const char* end = FindRenderedTextEnd(label);
        const std::string_view text(label, end - label);
        const float iconSize = S(16);
        const float textW = text.empty() ? 0.0f : Measure(Type::Button, text).x;
        const float contentW = textW + (icon != Icon::None ? iconSize + (text.empty() ? 0.0f : S(8)) : 0.0f);
        float x = std::round(bb.Min.x + (bb.GetWidth() - contentW) * 0.5f);
        if (icon != Icon::None)
        {
            DrawIcon(dl, icon, ImRect(ImVec2(x, bb.GetCenter().y - iconSize * 0.5f), ImVec2(x + iconSize, bb.GetCenter().y + iconSize * 0.5f)), ink);
            x += iconSize + S(8);
        }
        if (!text.empty())
            PaintClipped(dl, Type::Button, ImRect(ImVec2(x, bb.Min.y), ImVec2(std::min(bb.Max.x, x + textW + 2), bb.Max.y)), ink, text);
        if (KeyboardFocused(id))
            FocusRing(bb, rounding);
        return pressed;
    }

    bool IconButton(const char* id, Icon icon, ImVec2 size, bool danger)
    {
        ImGuiWindow* window = GetCurrentWindow();
        const ImGuiID iid = window->GetID(id);
        ImRect bb;
        if (!Place(iid, size, bb))
            return false;
        bool hovered = false, held = false;
        const bool pressed = ButtonBehavior(bb, iid, &hovered, &held);
        ImVec4& ink = StateFor(iid, c::text::text_hov);
        ink = Ease(ink, hovered || held ? (danger ? c::red : c::text::text_active) : c::text::text_hov);
        const float g = S(18);
        DrawIcon(GetWindowDrawList(), icon, ImRect(bb.GetCenter() - ImVec2(g, g) * 0.5f, bb.GetCenter() + ImVec2(g, g) * 0.5f), Col(ink));
        return pressed;
    }

    bool PageTile(const char* id, Icon icon, bool selected, ImVec2 size)
    {
        // The UI source's page() tab.
        ImGuiWindow* window = GetCurrentWindow();
        const ImGuiID iid = window->GetID(id);
        ImRect bb;
        if (!Place(iid, size, bb))
            return false;
        bool hovered = false, held = false;
        const bool pressed = ButtonBehavior(bb, iid, &hovered, &held);

        struct State { ImVec4 ink; float alpha; };
        State& s = StateFor(iid, State{ selected ? c::accent : c::text::text_hov, selected ? 1.0f : 0.0f });
        s.ink = Ease(s.ink, selected ? c::accent : (hovered ? c::text::text_active : c::text::text_hov));
        s.alpha = Ease(s.alpha, selected ? 1.0f : (hovered ? 0.3f : 0.0f));

        ImDrawList* dl = GetWindowDrawList();
        dl->AddRectFilled(bb.Min, bb.Max, Col(c::element::filling), S(c::control::choiceRounding));
        dl->AddRect(bb.Min, bb.Max, Col(c::accent, s.alpha), S(c::control::choiceRounding));
        const float g = S(24);
        DrawIcon(dl, icon, ImRect(bb.GetCenter() - ImVec2(g, g) * 0.5f, bb.GetCenter() + ImVec2(g, g) * 0.5f), Col(s.ink));
        if (KeyboardFocused(iid))
            FocusRing(bb, S(c::control::choiceRounding));
        return pressed;
    }

    bool Choice(const char* label, bool selected, ImVec2 size, bool enabled, const char* suffix)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID id = window->GetID(label);
        ImRect bb;
        if (!Place(id, ImVec2(RowWidth(size.x), std::round(S(size.y > 0 ? size.y : 32))), bb, enabled))
            return false;
        bool hovered = false, held = false;
        const bool pressed = ButtonBehavior(bb, id, &hovered, &held);

        struct State { ImVec4 ink; float alpha; };
        State& s = StateFor(id, State{ selected ? c::text::text_active : c::text::text, selected ? 1.0f : 0.0f });
        s.ink = Ease(s.ink, !enabled ? Fade(c::text::text, 0.5f) : (selected || hovered ? c::text::text_active : c::text::text));
        s.alpha = Ease(s.alpha, selected ? 1.0f : (hovered && enabled ? 0.35f : 0.0f));

        ImDrawList* dl = GetWindowDrawList();
        const float rounding = S(c::control::choiceRounding);
        dl->AddRectFilled(bb.Min, bb.Max, Col(selected ? c::control::selected : c::element::filling), rounding);
        dl->AddRect(bb.Min, bb.Max, Col(c::element::stroke), rounding);
        if (Mac())
        {
            if (!selected && s.alpha > 0.01f)
                dl->AddRectFilled(bb.Min, bb.Max, Col(c::control::selected, s.alpha), rounding);
        }
        else if (s.alpha > 0.01f)
        {
            dl->AddRect(bb.Min, bb.Max, Col(c::accent, s.alpha), rounding);
        }

        const char* end = FindRenderedTextEnd(label);
        const std::string_view text(label, end - label);
        const float textW = Measure(Type::Button, text).x;
        const float suffixW = (suffix && *suffix) ? Measure(Type::Small, suffix).x + S(6) : 0.0f;
        const float x = std::round(bb.Min.x + (bb.GetWidth() - textW - suffixW) * 0.5f);
        PaintClipped(dl, Type::Button, ImRect(ImVec2(std::max(bb.Min.x + S(6), x), bb.Min.y), ImVec2(std::min(bb.Max.x - S(6), x + textW + 1), bb.Max.y)), Col(s.ink), text);
        if (suffixW > 0)
            PaintClipped(dl, Type::Small, ImRect(ImVec2(x + textW + S(6), bb.Min.y), ImVec2(x + textW + suffixW, bb.Max.y)), Col(c::text::text_hov), suffix);
        if (KeyboardFocused(id))
            FocusRing(bb, S(c::control::choiceRounding));
        return pressed;
    }

    bool ChoiceGrid(const char* id, int* index, const std::vector<std::string>& items, int columns, float height, bool enabled)
    {
        PushID(id);
        bool changed = false;
        const float total = GetContentRegionAvail().x;
        const float gap = S(8);
        const float w = (total - gap * (columns - 1)) / columns;
        PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i % columns != 0)
                SameLine();
            PushID(static_cast<int>(i));
            if (Choice(items[i].c_str(), index && *index == static_cast<int>(i), ImVec2(w / Scale(), height), enabled) && index && *index != static_cast<int>(i))
            {
                *index = static_cast<int>(i);
                changed = true;
            }
            PopID();
        }
        PopStyleVar();
        PopID();
        return changed;
    }

    // ------------------------------------------------------------------ sliders

    std::string Percent(int v)
    {
        return std::to_string(v) + "%";
    }

    std::string Signed(int v)
    {
        if (v == 0)
            return "0";
        return v > 0 ? "+" + std::to_string(v) : "\xE2\x88\x92" + std::to_string(-v);
    }

    std::string Degrees(int v)
    {
        return (v > 0 ? "+" : "") + std::to_string(v) + "\xC2\xB0";
    }

    std::string Gamma(int v)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2f", v / 100.0);
        return buf;
    }

    namespace
    {
        // The shared track + knob behaviour. `track` is the rail's bounds.
        bool Rail(ImGuiID id, const ImRect& hitBox, const ImRect& track, int* value, const SliderSpec& spec, bool enabled, float knobRadius, bool& hovered, bool& held)
        {
            const bool pressed = ButtonBehavior(hitBox, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
            IM_UNUSED(pressed);
            const int range = std::max(1, spec.maximum - spec.minimum);
            const float span = std::max(1.0f, track.GetWidth());
            bool changed = false;
            if (enabled && value)
            {
                if (held)
                {
                    const float t = ImSaturate((GetIO().MousePos.x - track.Min.x) / span);
                    int raw = spec.minimum + static_cast<int>(std::lround(t * range));
                    if (spec.snap)
                        raw = Land(raw, spec.minimum, spec.maximum, spec.notch, spec.neutral);
                    if (raw != *value)
                    {
                        *value = raw;
                        changed = true;
                    }
                }
                if (hovered && IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    const int neutral = spec.neutral.value_or(spec.notch.value_or(spec.minimum));
                    changed |= *value != neutral;
                    *value = neutral;
                }
                if (changed)
                    MarkItemEdited(id);
            }

            ImDrawList* dl = GetWindowDrawList();
            const auto xOf = [&](int v) { return track.Min.x + (static_cast<float>(std::clamp(v, spec.minimum, spec.maximum) - spec.minimum) / range) * span; };
            const float cy = track.GetCenter().y;
            dl->AddRectFilled(track.Min, track.Max, Col(c::element::filling), S(30));
            dl->AddRect(track.Min, track.Max, Col(c::element::stroke), S(30));

            const int origin = spec.origin.value_or(spec.notch.value_or(spec.minimum));
            const float vx = value ? xOf(*value) : track.Min.x;
            const float ox = xOf(origin);
            float& fillAlpha = StateFor(id + 1, 1.0f);
            fillAlpha = Ease(fillAlpha, enabled ? ((hovered || held) ? 1.0f : 0.85f) : 0.3f);
            if (std::abs(vx - ox) > 0.5f)
                dl->AddRectFilled(ImVec2(std::min(ox, vx), track.Min.y), ImVec2(std::max(ox, vx), track.Max.y), Col(c::accent, fillAlpha), S(30));
            if (spec.notch && *spec.notch > spec.minimum && *spec.notch < spec.maximum)
            {
                const float nx = std::round(xOf(*spec.notch));
                dl->AddRectFilled(ImVec2(nx, cy - knobRadius - S(2)), ImVec2(nx + 1, cy + knobRadius + S(2)), Col(c::text::text_hov));
            }

            float& grow = StateFor(id + 2, 0.0f);
            grow = Ease(grow, enabled && (hovered || held) ? 1.0f : 0.0f, 10.0f);
            if (Mac())
            {
                const float r = knobRadius * 1.45f + S(0.5f) * grow;
                dl->AddCircleFilled(ImVec2(vx, cy + S(1)), r + S(0.5f), Col(ImVec4(0, 0, 0, 0.35f)), 0);
                dl->AddCircleFilled(ImVec2(vx, cy), r, Col(enabled ? c::control::knob : c::text::text), 0);
                if (KeyboardFocused(id))
                    dl->AddCircle(ImVec2(vx, cy), r + S(3), Col(c::accent, 0.8f), 0, S(1.5f));
                return changed;
            }
            const float r = knobRadius + S(1) * grow;
            dl->AddCircleFilled(ImVec2(vx, cy), r * 2.0f, Col(c::accent, 0.2f * fillAlpha), 0);
            dl->AddCircleFilled(ImVec2(vx, cy), r, Col(enabled ? c::control::knob : c::text::text), 0);
            if (KeyboardFocused(id))
                dl->AddCircle(ImVec2(vx, cy), r * 2.0f + S(2), Col(c::accent, 0.8f), 0, S(1.5f));
            return changed;
        }
    }

    bool Slider(const char* id, int* value, const SliderSpec& spec, bool enabled)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID iid = window->GetID(id);
        const float w = RowWidth(0);
        const float labelH = TextHeight(Type::Body);
        const float h = labelH + S(22);
        ImRect bb;
        if (!Place(iid, ImVec2(w, h), bb, enabled))
            return false;

        const float knob = S(5);
        const float half = S(Mac() ? 2.0f : 3.0f);  // macOS tracks are thin
        const ImRect track(ImVec2(bb.Min.x + knob, bb.Max.y - S(11) - half), ImVec2(bb.Max.x - knob, bb.Max.y - S(11) + half));
        bool hovered = false, held = false;
        const bool changed = Rail(iid, ImRect(ImVec2(bb.Min.x, bb.Min.y + labelH), bb.Max), track, value, spec, enabled, knob, hovered, held);

        ImVec4& labelInk = StateFor(iid + 3, c::text::text);
        labelInk = Ease(labelInk, enabled && (hovered || held) ? c::text::text_active : c::text::caption);
        ImDrawList* dl = GetWindowDrawList();
        const std::string reading = value ? spec.format(*value) : std::string();
        const float readingW = Measure(Type::Body, reading).x;
        PaintClipped(dl, Type::Body, ImRect(bb.Min, ImVec2(bb.Max.x - readingW - S(12), bb.Min.y + labelH)), Col(labelInk), spec.label);
        PaintClipped(dl, Type::Body, ImRect(ImVec2(bb.Max.x - readingW - 2, bb.Min.y), ImVec2(bb.Max.x, bb.Min.y + labelH)), Col(enabled ? c::text::text_active : c::text::text), reading, 1.0f);

        // The label carries the explanation.
        if (spec.tip)
        {
            const float lw = std::min(Measure(Type::Body, spec.label).x, bb.GetWidth());
            ItemAdd(ImRect(bb.Min, ImVec2(bb.Min.x + lw, bb.Min.y + labelH)), iid + 4);
            Tip(spec.tip);
        }
        return changed;
    }

    bool HeroSlider(const char* id, int* value, const SliderSpec& spec, const char* hint, std::span<const char* const> ticks, std::span<const float> tickFractions)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID iid = window->GetID(id);
        const float w = RowWidth(0);
        const float labelH = TextHeight(Type::Body);
        const float valueH = TextHeight(Type::Value);
        const float tickH = ticks.empty() ? 0.0f : TextHeight(Type::Small) + S(6);
        const float h = labelH + S(2) + valueH + S(10) + S(16) + tickH;
        ImRect bb;
        if (!Place(iid, ImVec2(w, h), bb))
            return false;

        ImDrawList* dl = GetWindowDrawList();
        PaintText(dl, Type::Body, bb.Min, Col(c::text::text_active), spec.label);
        if (hint && *hint)
        {
            const float hw = Measure(Type::Small, hint).x;
            PaintClipped(dl, Type::Small, ImRect(ImVec2(bb.Max.x - hw, bb.Min.y), ImVec2(bb.Max.x, bb.Min.y + labelH)), Col(c::text::text), hint, 1.0f);
        }

        // A large number with its unit set smaller in the accent, on the same baseline.
        const std::string reading = value ? spec.format(*value) : std::string();
        size_t lastDigit = std::string::npos;
        for (size_t i = 0; i < reading.size(); ++i)
        {
            if (reading[i] >= '0' && reading[i] <= '9')
                lastDigit = i;
        }
        const std::string number = lastDigit == std::string::npos ? reading : reading.substr(0, lastDigit + 1);
        const std::string unit = lastDigit == std::string::npos ? std::string() : reading.substr(lastDigit + 1);
        ImFont* big = Font(Type::Value);
        ImFont* small = Font(Type::Heading);
        const ImVec2 numberAt(bb.Min.x, std::round(bb.Min.y + labelH + S(2)));
        dl->AddText(big, big->FontSize, numberAt, Col(c::text::text_active), number.c_str());
        if (!unit.empty())
        {
            const float nx = numberAt.x + big->CalcTextSizeA(big->FontSize, FLT_MAX, 0, number.c_str()).x + S(3);
            dl->AddText(small, small->FontSize, ImVec2(std::round(nx), std::round(numberAt.y + big->Ascent - small->Ascent)), Col(c::accent), unit.c_str());
        }

        const float knob = S(6);
        const float trackY = bb.Min.y + labelH + S(2) + valueH + S(10) + S(8);
        const float half = S(Mac() ? 2.5f : 4.0f);  // macOS tracks are thin
        const ImRect track(ImVec2(bb.Min.x + knob, trackY - half), ImVec2(bb.Max.x - knob, trackY + half));
        bool hovered = false, held = false;
        const bool changed = Rail(iid, ImRect(ImVec2(bb.Min.x, trackY - S(12)), ImVec2(bb.Max.x, trackY + S(12))), track, value, spec, true, knob, hovered, held);

        if (!ticks.empty())
        {
            const float ty = trackY + S(16);
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                const float f = i < tickFractions.size() ? tickFractions[i] : (ticks.size() == 1 ? 0.0f : static_cast<float>(i) / (ticks.size() - 1));
                const float tw = Measure(Type::Small, ticks[i]).x;
                const float cx = track.Min.x + f * track.GetWidth();
                const float x = std::clamp(cx - tw * 0.5f, bb.Min.x, bb.Max.x - tw);
                PaintText(dl, Type::Small, ImVec2(x, ty), Col(c::text::text), ticks[i]);
            }
        }
        if (spec.tip)
        {
            ItemAdd(ImRect(bb.Min, ImVec2(bb.Min.x + Measure(Type::Body, spec.label).x, bb.Min.y + labelH)), iid + 4);
            Tip(spec.tip);
        }
        return changed;
    }

    // ------------------------------------------------------------------ inputs

    bool Combo(const char* id, int* index, const std::vector<std::string>& items, const char* placeholder, float width)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID iid = window->GetID(id);
        const ImGuiID popup = iid + 5;
        ImRect bb;
        const bool visible = Place(iid, ImVec2(RowWidth(width), std::round(S(36))), bb, !items.empty());
        bool hovered = false, held = false;
        bool pressed = false;
        if (visible)
            pressed = ButtonBehavior(bb, iid, &hovered, &held);
        const bool open = IsPopupOpen(popup, ImGuiPopupFlags_None);
        if (pressed && !open)
            OpenPopupEx(popup, ImGuiPopupFlags_None);

        if (visible)
        {
            float& edge = StateFor(iid, 0.0f);
            edge = Ease(edge, open ? 0.7f : (hovered ? 0.35f : 0.0f));
            ImDrawList* dl = GetWindowDrawList();
            dl->AddRectFilled(bb.Min, bb.Max, Col(c::element::filling), S(c::control::choiceRounding));
            dl->AddRect(bb.Min, bb.Max, Col(c::element::stroke), S(c::control::choiceRounding));
            if (edge > 0.01f)
                dl->AddRect(bb.Min, bb.Max, Col(c::accent, edge), S(c::control::choiceRounding));
            const bool has = index && *index >= 0 && *index < static_cast<int>(items.size());
            PaintClipped(dl, Type::Body, ImRect(ImVec2(bb.Min.x + S(12), bb.Min.y), ImVec2(bb.Max.x - bb.GetHeight(), bb.Max.y)),
                Col(has ? c::text::text_active : c::text::text), has ? items[*index] : placeholder);
            const float g = S(16);
            const ImVec2 cc(bb.Max.x - bb.GetHeight() * 0.5f, bb.GetCenter().y);
            DrawIcon(dl, Icon::ChevronDown, ImRect(cc - ImVec2(g, g) * 0.5f, cc + ImVec2(g, g) * 0.5f), Col(c::accent));
            if (KeyboardFocused(iid))
                FocusRing(bb, S(c::control::choiceRounding));
        }

        bool changed = false;
        const float rowH = std::round(S(30));
        const float pad = S(5);
        const float listH = std::min(rowH * items.size() + pad * 2, S(300));
        SetNextWindowPos(ImVec2(bb.Min.x, bb.Max.y + S(4)));
        SetNextWindowSize(ImVec2(bb.GetWidth(), listH));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
        PushStyleVar(ImGuiStyleVar_PopupRounding, S(c::control::choiceRounding));
        PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        PushStyleColor(ImGuiCol_PopupBg, c::element::popup);
        PushStyleColor(ImGuiCol_Border, c::element::stroke);
        if (BeginPopupEx(popup, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImDrawList* pdl = GetWindowDrawList();
            const float w = GetContentRegionAvail().x;
            for (size_t i = 0; i < items.size(); ++i)
            {
                PushID(static_cast<int>(i));
                const ImGuiID rid = GetID("row");
                ImRect row;
                if (Place(rid, ImVec2(w, rowH), row))
                {
                    bool rh = false, rheld = false;
                    const bool chosen = ButtonBehavior(row, rid, &rh, &rheld);
                    const bool selected = index && *index == static_cast<int>(i);
                    float& hot = StateFor(rid, 0.0f);
                    hot = Ease(hot, rh ? 1.0f : 0.0f, 12.0f);
                    if (hot > 0.01f)
                        pdl->AddRectFilled(row.Min, row.Max, Col(c::accent, 0.08f * hot), S(3));
                    PaintClipped(pdl, Type::Body, ImRect(ImVec2(row.Min.x + S(8), row.Min.y), ImVec2(row.Max.x - S(28), row.Max.y)),
                        Col(selected ? c::accent : Lerp(c::text::caption, c::text::text_active, hot)), items[i]);
                    if (selected)
                    {
                        const float g = S(14);
                        const ImVec2 cc(row.Max.x - S(14), row.GetCenter().y);
                        DrawIcon(pdl, Icon::Check, ImRect(cc - ImVec2(g, g) * 0.5f, cc + ImVec2(g, g) * 0.5f), Col(c::accent));
                    }
                    if (chosen)
                    {
                        if (index && *index != static_cast<int>(i))
                        {
                            *index = static_cast<int>(i);
                            changed = true;
                        }
                        CloseCurrentPopup();
                    }
                }
                PopID();
            }
            EndPopup();
        }
        PopStyleColor(2);
        PopStyleVar(3);
        return changed;
    }

    bool TextField(const char* id, char* buffer, size_t size, const char* hint, Icon icon, ImGuiInputTextFlags flags, bool* submitted, Type font, float width)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const float w = RowWidth(width);
        const float h = std::round(S(38));
        const ImVec2 pos = window->DC.CursorPos;
        FontScope scope(font);
        const float pad = std::floor((h - TextHeight(font)) * 0.5f);
        PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad + S(2), pad));
        ImRadiance::FieldIconReserve = icon != Icon::None ? h : 0.0f;
        const bool edited = InputTextEx(id, hint, buffer, static_cast<int>(size), ImVec2(w, h), flags);
        ImRadiance::FieldIconReserve = 0.0f;
        if (submitted)
            *submitted = IsItemDeactivated() && (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter));
        PopStyleVar();
        if (icon != Icon::None)
        {
            const float g = S(16);
            const ImVec2 cc(pos.x + w - h * 0.5f, pos.y + h * 0.5f);
            DrawIcon(GetWindowDrawList(), icon, ImRect(cc - ImVec2(g, g) * 0.5f, cc + ImVec2(g, g) * 0.5f), Col(c::accent));
        }
        return edited;
    }

    bool IsCapturingHotkey()
    {
        return g_capturing != 0;
    }

    bool HotkeyField(const char* id, Hotkey* hotkey, float width)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID iid = window->GetID(id);
        ImRect bb;
        const bool visible = Place(iid, ImVec2(RowWidth(width), std::round(S(38))), bb);
        bool hovered = false, held = false;
        bool pressed = false;
        if (visible)
            pressed = ButtonBehavior(bb, iid, &hovered, &held);
        bool changed = false;
        const bool capturing = g_capturing == iid;

        if (pressed)
            g_capturing = capturing ? 0 : iid;
        else if (capturing)
        {
            const ImGuiIO& io = GetIO();
            if (IsKeyPressed(ImGuiKey_Escape, false))
                g_capturing = 0;
            else if (IsKeyPressed(ImGuiKey_Backspace, false))
            {
                if (hotkey)
                {
                    hotkey->virtualKey = 0;
                    hotkey->modifiers = 0;
                    changed = true;
                }
                g_capturing = 0;
            }
            else
            {
                for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
                {
                    const ImGuiKey key = static_cast<ImGuiKey>(k);
                    if (!IsKeyPressed(key, false))
                        continue;
                    const uint32_t vk = VkFromImGuiKey(key);
                    if (vk == 0)
                        continue;
                    if (hotkey)
                    {
                        hotkey->modifiers = (io.KeyAlt ? 1u : 0u) | (io.KeyCtrl ? 2u : 0u) | (io.KeyShift ? 4u : 0u) | (io.KeySuper ? 8u : 0u);
                        hotkey->virtualKey = vk;
                        hotkey->enabled = true;
                        changed = true;
                    }
                    g_capturing = 0;
                    break;
                }
            }
            if (g_capturing == iid && IsMouseClicked(ImGuiMouseButton_Left) && !hovered)
                g_capturing = 0;
        }
        if (!visible)
            return changed;

        float& edge = StateFor(iid, 0.0f);
        edge = Ease(edge, g_capturing == iid ? 0.8f : (hovered ? 0.35f : 0.0f));
        ImDrawList* dl = GetWindowDrawList();
        dl->AddRectFilled(bb.Min, bb.Max, Col(c::element::filling), S(c::control::choiceRounding));
        dl->AddRect(bb.Min, bb.Max, Col(c::element::stroke), S(c::control::choiceRounding));
        if (edge > 0.01f)
            dl->AddRect(bb.Min, bb.Max, Col(c::accent, edge), S(c::control::choiceRounding));
        std::string text;
        ImVec4 ink = c::text::text_active;
        if (g_capturing == iid)
        {
            text = "Press a shortcut\xE2\x80\xA6";
            ink = c::accent;
        }
        else
        {
            text = hotkey ? hotkey->Describe() : "Not set";
            if (!hotkey || !hotkey->IsSet())
                ink = c::text::text;
        }
        PaintClipped(dl, Type::Body, ImRect(ImVec2(bb.Min.x + S(12), bb.Min.y), ImVec2(bb.Max.x - bb.GetHeight(), bb.Max.y)), Col(ink), text);
        const float g = S(16);
        const ImVec2 cc(bb.Max.x - bb.GetHeight() * 0.5f, bb.GetCenter().y);
        DrawIcon(dl, Icon::Keycap, ImRect(cc - ImVec2(g, g) * 0.5f, cc + ImVec2(g, g) * 0.5f), Col(c::accent));
        if (KeyboardFocused(iid))
            FocusRing(bb, S(c::control::choiceRounding));
        if (g_capturing != iid)
        {
            SetItemTooltip("Click, then press the keys. Esc cancels, Backspace clears.");
        }
        return changed;
    }

    // ------------------------------------------------------------------ cards

    CardResult Card(const char* id, const CardSpec& spec)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return CardResult::None;
        const ImGuiID iid = window->GetID(id);
        const float w = RowWidth(0);
        const float h = std::round(S(spec.height));
        ImRect bb;
        if (!Place(iid, ImVec2(w, h), bb))
            return CardResult::None;

        const float zone = S(22);
        const ImRect removeZone(ImVec2(bb.Max.x - S(10) - zone, bb.Min.y + S(8)), ImVec2(bb.Max.x - S(10), bb.Min.y + S(8) + zone));
        bool hovered = false, held = false;
        const bool pressed = ButtonBehavior(bb, iid, &hovered, &held);
        const bool overRemove = spec.removable && hovered && removeZone.Contains(GetIO().MousePos);

        struct State { ImVec4 stroke; float lift; };
        State& s = StateFor(iid, State{ spec.selected ? c::accent : c::element::stroke, 0.0f });
        s.stroke = Ease(s.stroke, spec.selected ? c::accent : (hovered ? Fade(c::accent, 0.35f) : c::element::stroke));
        s.lift = Ease(s.lift, hovered ? 1.0f : 0.0f);

        ImDrawList* dl = GetWindowDrawList();
        const float rounding = S(c::element::rounding);
        dl->AddRectFilled(bb.Min, bb.Max, Col(c::element::filling), rounding);
        if (spec.selected || s.lift > 0.01f)
            dl->AddRectFilled(bb.Min, bb.Max, Col(c::accent, 0.04f * std::max(s.lift, spec.selected ? 1.0f : 0.0f)), rounding);
        dl->AddRect(bb.Min, bb.Max, Col(s.stroke), rounding);

        // Picture or icon on the right.
        float textRight = bb.Max.x - S(20);
        if (spec.image)
        {
            const float ih = h - S(24);
            const float iw = std::round(ih * 16.0f / 9.0f);
            const ImRect pic(ImVec2(bb.Max.x - S(14) - iw, bb.Min.y + S(12)), ImVec2(bb.Max.x - S(14), bb.Max.y - S(12)));
            const float crop = (1.0f - (16.0f / 9.0f) / (384.0f / 180.0f)) * 0.5f;
            dl->AddImageRounded(spec.image, pic.Min, pic.Max, ImVec2(crop, 0), ImVec2(1.0f - crop, 1), Col(ImVec4(1, 1, 1, 1)), S(6));
            dl->AddRect(pic.Min, pic.Max, Col(c::element::stroke), S(6));
            textRight = pic.Min.x - S(14);
        }
        else if (spec.icon != Icon::None)
        {
            const float g = S(30);
            const ImVec2 cc(bb.Max.x - S(30) - g * 0.5f, bb.GetCenter().y);
            DrawIcon(dl, spec.icon, ImRect(cc - ImVec2(g, g) * 0.5f, cc + ImVec2(g, g) * 0.5f), Col(c::accent));
            textRight = cc.x - g * 0.5f - S(16);
        }

        if (spec.removable && hovered)
        {
            if (overRemove)
                dl->AddCircleFilled(removeZone.GetCenter(), zone * 0.5f, Col(c::red, 0.18f), 0);
            const float g = S(14);
            DrawIcon(dl, Icon::Close, ImRect(removeZone.GetCenter() - ImVec2(g, g) * 0.5f, removeZone.GetCenter() + ImVec2(g, g) * 0.5f), Col(overRemove ? c::red : c::text::text_hov));
        }

        // Title (with its tag) over the info line, centred as a block.
        const float titleH = TextHeight(Type::Body);
        const float infoH = spec.info.empty() ? 0.0f : TextHeight(Type::BodyRegular);
        const float gap = spec.info.empty() ? 0.0f : S(6);
        const float top = std::round(bb.GetCenter().y - (titleH + gap + infoH) * 0.5f);
        const float left = bb.Min.x + S(20);
        const float titleW = std::min(Measure(Type::Body, spec.title).x, textRight - left);
        PaintClipped(dl, Type::Body, ImRect(ImVec2(left, top), ImVec2(left + titleW + 1, top + titleH)), Col(c::text::text_active), spec.title);
        if (!spec.tag.empty())
            PaintClipped(dl, Type::Body, ImRect(ImVec2(left + titleW + S(8), top), ImVec2(textRight, top + titleH)), Col(c::text::text_hov), spec.tag);
        if (!spec.info.empty())
            PaintClipped(dl, Type::BodyRegular, ImRect(ImVec2(left, top + titleH + gap), ImVec2(textRight, top + titleH + gap + infoH)), Col(c::text::text), spec.info);

        if (KeyboardFocused(iid))
            FocusRing(bb, rounding);
        if (pressed)
            return overRemove ? CardResult::Removed : CardResult::Clicked;
        return CardResult::None;
    }

    // ------------------------------------------------------------------ misc

    void ProgressRing(ImDrawList* dl, ImVec2 centre, float radius, float progress, float thickness)
    {
        constexpr float kPi = 3.14159265f;
        dl->PathClear();
        dl->PathArcTo(centre, radius, 0.0f, 2.0f * kPi, 64);
        dl->PathStroke(Col(c::element::stroke), ImDrawFlags_None, S(thickness));
        dl->PathClear();
        dl->PathArcTo(centre, radius, kPi * 1.5f, kPi * 1.5f + ImSaturate(progress) * 2.0f * kPi, 64);
        dl->PathStroke(Col(c::accent), ImDrawFlags_None, S(thickness));
    }

    void OpenDialog(const char* id)
    {
        OpenPopup(id);
    }

    bool BeginDialog(const char* id, float logicalWidth)
    {
        const ImGuiViewport* vp = GetMainViewport();
        SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        SetNextWindowSize(ImVec2(std::round(S(logicalWidth)), 0));
        PushStyleVar(ImGuiStyleVar_WindowPadding, S(22, 20));
        PushStyleVar(ImGuiStyleVar_ItemSpacing, S(10, 14));
        PushStyleVar(ImGuiStyleVar_PopupRounding, S(c::background::rounding));
        PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
        PushStyleColor(ImGuiCol_PopupBg, c::background::filling);
        PushStyleColor(ImGuiCol_Border, c::background::stroke);
        const bool open = BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
        PopStyleColor(2);
        PopStyleVar(2);
        if (!open)
        {
            PopStyleVar(2);
            return false;
        }
        return true;
    }

    void CloseDialog()
    {
        CloseCurrentPopup();
    }

    void EndDialog()
    {
        EndPopup();
        PopStyleVar(2);
    }
}
