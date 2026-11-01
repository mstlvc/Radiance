#include "ui/pages/ResolutionPage.h"

#include "core/Catalog.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

namespace radiance::ui
{
    ResolutionPage::ResolutionPage(PageContext& context) : Page(context) {}

    void ResolutionPage::OnShow()
    {
        Refresh();
    }

    std::string ResolutionPage::Device() const
    {
        return displays_.empty() ? std::string() : displays_[static_cast<size_t>(displayIndex_)].deviceName;
    }

    void ResolutionPage::Refresh()
    {
        IDisplayService& d = *ctx_.services.displays;
        const std::string previous = Device();
        displays_ = d.Displays();
        displayIndex_ = 0;
        for (size_t i = 0; i < displays_.size(); ++i)
        {
            if (displays_[i].deviceName == previous)
                displayIndex_ = static_cast<int>(i);
        }
        ShowDisplay(displayIndex_);
        if (current_ && !customWidth_[0])
        {
            std::snprintf(customWidth_, sizeof(customWidth_), "%d", current_->width);
            std::snprintf(customHeight_, sizeof(customHeight_), "%d", current_->height);
            std::snprintf(customRate_, sizeof(customRate_), "%d", current_->refreshHz);
        }
    }

    void ResolutionPage::ShowDisplay(int index)
    {
        IDisplayService& d = *ctx_.services.displays;
        displayIndex_ = index;
        modes_ = d.Modes(Device());
        current_ = d.Current(Device());
        native_ = d.Native(Device());
        integerScaling_ = d.IntegerScalingAvailable();
        customResolutions_ = d.CustomResolutionsAvailable();
        refreshedAt_ = ImGui::GetTime();
    }

    std::optional<DisplayMode> ResolutionPage::Native() const
    {
        if (native_)
            return native_;
        std::optional<DisplayMode> best;
        for (const DisplayMode& m : modes_)
        {
            if (!best || static_cast<long long>(m.width) * m.height > static_cast<long long>(best->width) * best->height)
                best = m;
        }
        return best;
    }

    std::vector<DisplayMode> ResolutionPage::BestPerResolution(size_t limit) const
    {
        std::map<std::pair<int, int>, DisplayMode> best;
        for (const DisplayMode& m : modes_)
        {
            auto& slot = best[{ m.width, m.height }];
            if (m.refreshHz > slot.refreshHz)
                slot = m;
        }
        std::vector<DisplayMode> list;
        for (const auto& [size, mode] : best)
            list.push_back(mode);
        std::sort(list.begin(), list.end(), [](const DisplayMode& a, const DisplayMode& b) {
            return a.width != b.width ? a.width > b.width : a.height > b.height;
        });
        if (list.size() > limit)
            list.resize(limit);
        return list;
    }

    std::vector<int> ResolutionPage::RatesFor(int width, int height) const
    {
        std::set<int, std::greater<int>> rates;
        for (const DisplayMode& m : modes_)
        {
            if (m.width == width && m.height == height && m.refreshHz > 1)
                rates.insert(m.refreshHz);
        }
        return std::vector<int>(rates.begin(), rates.end());
    }

    void ResolutionPage::Apply(const DisplayMode& mode)
    {
        // Success shows as the keep-or-go-back countdown; only a refusal needs words here.
        const Outcome o = ctx_.services.displays->Apply(Device(), mode, ctx_.settings.scaling);
        statusGood_ = o.ok;
        status_ = o.ok ? std::string() : (o.message.empty() ? "Your monitor refused " + ModeText(mode) + ". Nothing changed." : o.message);
        Refresh();
    }

    void ResolutionPage::Draw(ImVec2 area)
    {
        if (refreshedAt_ < 0 || ImGui::GetTime() - refreshedAt_ > 3.0)
            Refresh();
        TwoPanels(area, 0.5f, [&] { DrawModes(); }, [&] { DrawPresetsAndRules(); });
    }

    void ResolutionPage::DrawNow()
    {
        // What the display is running now, and how that sits on the panel.
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = S(92);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(w, h));
        ImDrawList* dl = ImGui::GetWindowDrawList();

        const std::string where = displays_.empty() ? std::string("Display 1")
                                                    : "Display " + std::to_string(displayIndex_ + 1) + " \xC2\xB7 " + displays_[static_cast<size_t>(displayIndex_)].friendlyName;
        const float boxW = S(124), boxH = S(70);
        PaintClipped(dl, Type::Small, ImRect(at, ImVec2(at.x + w - boxW - S(16), at.y + TextHeight(Type::Small))), Col(c::text::text), "Now on " + where);
        if (current_)
        {
            PaintText(dl, Type::Value, ImVec2(at.x, at.y + S(18)), Col(c::text::text_active), ModeText(*current_));
            PaintText(dl, Type::BodyRegular, ImVec2(at.x, at.y + S(62)), Col(c::accent),
                std::to_string(current_->refreshHz) + " Hz \xC2\xB7 " + AspectLabel(current_->width, current_->height) + " \xC2\xB7 " + Megapixels(current_->width, current_->height));
        }
        else
        {
            PaintText(dl, Type::BodyRegular, ImVec2(at.x, at.y + S(30)), Col(c::text::caption), "Windows didn't report a mode.");
        }

        const ImRect box(ImVec2(at.x + w - boxW, at.y + S(4)), ImVec2(at.x + w, at.y + S(4) + boxH));
        dl->AddRectFilled(box.Min, box.Max, Col(c::element::filling), S(4));
        dl->AddRect(box.Min, box.Max, Col(c::element::stroke), S(4));
        std::string fills = "fills the panel";
        const std::optional<DisplayMode> native = Native();
        if (current_ && native && native->height > 0 && current_->height > 0)
        {
            const float panelAspect = static_cast<float>(native->width) / native->height;
            const float modeAspect = static_cast<float>(current_->width) / current_->height;
            const ScalingMode scaling = ctx_.services.displays->Scaling(Device()).value_or(ctx_.settings.scaling.value_or(ScalingMode::AspectRatio));
            ImRect fitted(box.Min + S(6, 6), box.Max - S(6, 6));
            if (std::abs(panelAspect - modeAspect) > 0.02f && scaling != ScalingMode::FullScreen)
            {
                const ImVec2 size(fitted.GetWidth() * std::min(1.0f, modeAspect / panelAspect), fitted.GetHeight() * std::min(1.0f, panelAspect / modeAspect));
                fitted = ImRect(fitted.GetCenter() - size * 0.5f, fitted.GetCenter() + size * 0.5f);
                fills = modeAspect < panelAspect ? "bars at the sides" : "bars top and bottom";
            }
            else if (std::abs(panelAspect - modeAspect) > 0.02f)
            {
                fills = "stretched to fill";
            }
            dl->AddRectFilled(fitted.Min, fitted.Max, Col(c::accent, 0.12f), S(2));
            dl->AddRect(fitted.Min, fitted.Max, Col(c::accent, 0.8f), S(2));
            PaintClipped(dl, Type::SmallMedium, fitted, Col(c::text::text_active), AspectLabel(current_->width, current_->height), 0.5f);
        }
        PaintClipped(dl, Type::Small, ImRect(ImVec2(box.Min.x, box.Max.y + S(2)), ImVec2(box.Max.x, box.Max.y + S(18))), Col(c::text::text), fills, 0.5f);
    }

    // ------------------------------------------------------------------ left: modes

    void ResolutionPage::DrawModes()
    {
        DrawNow();
        if (displays_.size() > 1)
        {
            std::vector<std::string> names;
            for (const DisplayInfo& d : displays_)
                names.push_back(d.friendlyName + (d.primary ? "  (primary)" : ""));
            int index = displayIndex_;
            if (Combo("##display", &index, names))
                ShowDisplay(index);
        }

        Separator();
        const std::vector<DisplayMode> list = BestPerResolution(12);
        SectionTitle("Resolution", std::to_string(list.size()) + (list.size() == 1 ? " mode" : " modes"));
        {
            const float w = Split(3);
            for (size_t i = 0; i < list.size(); ++i)
            {
                if (i % 3)
                    ImGui::SameLine();
                const DisplayMode& m = list[i];
                char id[64];
                std::snprintf(id, sizeof(id), "%s##mode%zu", ModeText(m).c_str(), i);
                const std::string aspect = AspectLabel(m.width, m.height);
                if (Choice(id, current_ && current_->SameSize(m), ImVec2(w, 34), true, aspect == "4:3" ? "4:3" : nullptr))
                    Apply(m);  // at its highest refresh rate
            }
        }

        SectionTitle("Refresh rate", "Highest first");
        if (current_)
        {
            const std::vector<int> rates = RatesFor(current_->width, current_->height);
            const float w = Split(4);
            for (size_t i = 0; i < rates.size() && i < 8; ++i)
            {
                if (i % 4)
                    ImGui::SameLine();
                char id[32];
                std::snprintf(id, sizeof(id), "%d Hz##rate", rates[i]);
                if (Choice(id, rates[i] == current_->refreshHz, ImVec2(w, 34)))
                    Apply(DisplayMode{ current_->width, current_->height, rates[i] });
            }
        }

        Separator();
        SectionTitle("Custom mode");
        Caption(customResolutions_ ? "Any width, height and refresh rate. One your display doesn't list is made with the NVIDIA driver, and only saved if you keep it."
                                   : "Any width, height and refresh rate your display offers.");
        const float field = Split(3);
        bool s1 = false, s2 = false, s3 = false;
        TextField("##customWidth", customWidth_, sizeof(customWidth_), "Width", Icon::None, ImGuiInputTextFlags_CharsDecimal, &s1, Type::Body, field);
        ImGui::SameLine();
        TextField("##customHeight", customHeight_, sizeof(customHeight_), "Height", Icon::None, ImGuiInputTextFlags_CharsDecimal, &s2, Type::Body, field);
        ImGui::SameLine();
        TextField("##customRate", customRate_, sizeof(customRate_), "Hz", Icon::None, ImGuiInputTextFlags_CharsDecimal, &s3, Type::Body, field);
        if (Button("Apply custom mode##custom", ImVec2(0, 34), ButtonKind::Accent, Icon::Check) || s1 || s2 || s3)
        {
            const DisplayMode m{ std::atoi(customWidth_), std::atoi(customHeight_), std::atoi(customRate_) };
            if (m.width < 640 || m.height < 480 || m.refreshHz < 24 || m.width > 15360 || m.height > 8640 || m.refreshHz > 1000)
            {
                status_ = "That isn't a mode a monitor could show - check the numbers.";
                statusGood_ = false;
            }
            else
            {
                Apply(m);
            }
        }
        if (!status_.empty())
            Note(statusGood_ ? "Done" : "Not applied", status_);
    }

    // ------------------------------------------------------------------ right: presets and rules

    void ResolutionPage::DrawPresetsAndRules()
    {
        AppSettings& s = ctx_.settings;
        SectionTitle("PvP presets", "Rust, CS2, Fortnite");
        const std::optional<DisplayMode> native = Native();
        for (const PvpResolution& p : catalog::PvpResolutions())
        {
            DisplayMode mode{ p.width, p.height, 0 };
            if (!p.needsStretching && native)
                mode = DisplayMode{ native->width, native->height, 0 };
            const bool inUse = current_ && current_->SameSize(mode);
            CardSpec card;
            card.title = p.name;
            card.tag = ModeText(mode);
            card.info = p.why;
            card.selected = inUse;
            card.icon = Icon::Resolution;
            card.height = 74;
            char id[48];
            std::snprintf(id, sizeof(id), "##pvp%s", p.name);
            if (Card(id, card) == CardResult::Clicked && !inUse)
            {
                if (current_)
                    mode.refreshHz = current_->refreshHz;
                Apply(mode);
            }
            Tip((std::string(p.why) + " " + p.tradeOff).c_str());
        }
        Caption("4:3 only stretches when scaling is Full screen. On Aspect ratio or No scaling you get black bars at the sides instead.");

        Separator();
        SectionTitle("Scaling");
        Caption(std::string("How a resolution below the monitor's native one fills the screen. Radiance puts your choice on every resolution it switches to.") +
            (displays_.size() > 1 ? " Applies to every display." : ""));
        IDisplayService& d = *ctx_.services.displays;
        const auto choices = catalog::ScalingChoices();
        // What Windows is doing now when something is being scaled, otherwise the saved choice.
        const std::optional<ScalingMode> scaling = d.Scaling(Device()) ? d.Scaling(Device()) : s.scaling;
        const float half = Split(2);
        for (size_t i = 0; i < choices.size(); ++i)
        {
            if (i % 2)
                ImGui::SameLine();
            const bool unavailable = choices[i].mode == ScalingMode::Integer && !integerScaling_;
            char id[64];
            std::snprintf(id, sizeof(id), "%s##scaling", choices[i].label);
            const bool picked = Choice(id, scaling == choices[i].mode, ImVec2(half, 34), !unavailable, unavailable ? (customResolutions_ ? "Not here" : "NVIDIA only") : nullptr);
            if (unavailable)
            {
                Tip(customResolutions_ ? "The NVIDIA driver won't use integer scaling on this display right now."
                                       : "Integer scaling is an NVIDIA feature (GTX 16 and RTX cards).");
            }
            if (picked)
            {
                s.scaling = choices[i].mode;
                ctx_.host.SettingsChanged();
                const Outcome o = d.SetScaling(choices[i].mode);
                statusGood_ = o.ok;
                status_ = o.message;
            }
        }

        Separator();
        SectionTitle("When a game launches");
        if (Toggle("Per-game resolution rules##rules", &s.monitorRulesEnabled, "Switch to a game's resolution when it opens, and back when it closes."))
            ctx_.host.SettingsChanged();
        if (GameCombo("##ruleGame", s.selectedGameId))
            ctx_.host.SettingsChanged();
        const SupportedGame* game = catalog::GameById(s.selectedGameId);
        const MonitorRule* rule = game ? s.RuleFor(game->id) : nullptr;
        if (!game)
            Caption("Pick a game, then the resolution it should launch at.");
        else if (!rule)
            Caption(std::string(game->displayName) + " currently leaves your resolution alone.");
        else
            Caption(std::string(game->displayName) + " switches to " + std::to_string(rule->width) + " \xC3\x97 " + std::to_string(rule->height) + ", and back on exit.");

        std::vector<std::string> options{ "Don't change" };
        const std::vector<DisplayMode> list = BestPerResolution(11);
        int selected = 0;
        for (size_t i = 0; i < list.size(); ++i)
        {
            options.push_back(ModeText(list[i]));
            if (rule && rule->width == list[i].width && rule->height == list[i].height)
                selected = static_cast<int>(i) + 1;
        }
        if (ChoiceGrid("##ruleModes", &selected, options, 3, 32, game != nullptr) && game)
        {
            if (selected == 0)
                s.SetRule(game->id, 0, 0);
            else
                s.SetRule(game->id, list[static_cast<size_t>(selected - 1)].width, list[static_cast<size_t>(selected - 1)].height);
            ctx_.host.SettingsChanged();
        }

        Separator();
        SectionTitle("HDR");
        const bool hdr = !displays_.empty() && displays_[static_cast<size_t>(displayIndex_)].hdrActive;
        InfoRow("HDR on this display", hdr ? "On" : "Off", hdr ? c::orange : c::green);
        Caption(hdr ? "Windows ignores gamma-ramp colour changes while HDR is on, so saturation and vibrance carry the look."
                    : "HDR is off. Your colour controls all work.");
    }
}
