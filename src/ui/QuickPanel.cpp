#include "ui/QuickPanel.h"

#include "core/Catalog.h"
#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include "imgui_internal.h"

#include <cmath>

namespace radiance::ui
{
    using namespace QuickMetrics;

    bool QuickPanel::IsCaption(ImVec2 point, ImVec2 size) const
    {
        return point.y >= 0 && point.y < S(Header) && point.x < size.x - S(Header);
    }

    void QuickPanel::ApplyLook(const LookPreset& preset)
    {
        AppSettings& s = ctx_.settings;
        const ColourVision keepVision = s.look.vision;  // colour vision is the person's, not the look's
        s.look = preset.look;
        s.look.vision = keepVision;
        s.lookName = preset.name;
        ctx_.host.ColourChanged();
    }

    void QuickPanel::Reset()
    {
        AppSettings& s = ctx_.settings;
        const ColourVision keepVision = s.look.vision;
        s.look = ColourLook{};
        s.look.vibrance = ctx_.services.colour->DriverDefault();
        s.look.vision = keepVision;
        s.lookName.clear();
        ctx_.host.ColourChanged();
    }

    void QuickPanel::Draw(ImVec2 size)
    {
        AppSettings& s = ctx_.settings;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Header: the mark, the name, and a close button.
        const float rounding = S(c::background::rounding);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(size.x, S(Header)), Col(c::element::panel), rounding, ImDrawFlags_RoundCornersTop);
        DrawMark(dl, ImVec2(S(24), S(Header) * 0.5f), S(18), Col(c::accent), Col(c::accent, 0.08f));
        const float titleH = TextHeight(Type::Heading);
        PaintText(dl, Type::Heading, ImVec2(S(44), std::round((S(Header) - titleH) * 0.5f)), Col(c::text::text_active), "Quick colour");
        ImGui::SetCursorScreenPos(ImVec2(size.x - S(Header), 0));
        if (IconButton("##quickClose", Icon::Close, ImVec2(S(Header), S(Header))))
            ctx_.host.HideWindow();

        // Body.
        const float pad = S(18);
        ImGui::SetCursorScreenPos(ImVec2(pad, S(Header) + S(12)));
        ImGui::BeginGroup();
        ImGui::PushItemWidth(size.x - pad * 2);
        ImGui::BeginChild("##quickBody", ImVec2(size.x - pad * 2, size.y - S(Header) - S(12) - pad), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

        if (const GameProfile* active = s.activeProfile.empty() ? nullptr : s.ProfileFor(s.activeProfile))
            Caption(active->displayName + " profile \xC2\xB7 changes are saved to it", c::accent);

        if (SaturationAndVibrance(ctx_))
        {
            s.lookName.clear();
            ctx_.host.ColourChanged();
        }

        // Scene looks: the running game's, else the game the Display page last showed,
        // else the first game that has some.
        std::string lookGame = s.lookGame;
        if (const SupportedGame* running = ctx_.services.games->Running(); running && running->lookGame)
            lookGame = running->lookGame;
        for (const SupportedGame& game : catalog::Games())
        {
            if (!lookGame.empty())
                break;
            if (game.lookGame)
                lookGame = game.lookGame;
        }
        const std::vector<LookPreset> looks = lookGame.empty() ? std::vector<LookPreset>{} : catalog::LooksFor(lookGame);
        if (!looks.empty())
        {
            SectionTitle("Scene looks", lookGame);
            std::vector<std::string> names;
            int selected = -1;
            for (size_t i = 0; i < looks.size(); ++i)
            {
                names.push_back(looks[i].name);
                if (looks[i].name == s.lookName)
                    selected = static_cast<int>(i);
            }
            const int was = selected;
            if (ChoiceGrid("##quickLooks", &selected, names, 3, 30) && selected >= 0 && selected != was)
            {
                s.lookGame = lookGame;
                ApplyLook(looks[static_cast<size_t>(selected)]);
            }
        }

        const float half = Split(2);
        if (Button("Reset##quick", ImVec2(half, 34), ButtonKind::Normal, Icon::Reset))
            Reset();
        ImGui::SameLine();
        if (Button("Open Radiance##quick", ImVec2(half, 34), ButtonKind::Accent, Icon::ExternalLink))
            ctx_.host.ShowPage(PageId::Display);

        Caption(s.quickPopup.Describe() + " shows and hides this. Esc closes it.");

        ImGui::EndChild();
        ImGui::PopItemWidth();
        ImGui::EndGroup();
    }
}
