#include "ui/pages/UtilitiesPage.h"

#include "core/Catalog.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include <algorithm>
#include <cstdio>

namespace radiance::ui
{
    void UtilitiesPage::Draw(ImVec2 area)
    {
        TwoPanels(area, 0.46f, [&] { DrawList(); }, [&] { DrawDetail(); });
    }

    void UtilitiesPage::DrawList()
    {
        const auto tools = catalog::Utilities();
        SectionTitle("Tools", std::to_string(tools.size()) + (tools.size() == 1 ? " tool" : " tools"));
        for (size_t i = 0; i < tools.size(); ++i)
        {
            CardSpec card;
            card.title = tools[i].name;
            card.tag = tools[i].author;
            card.info = tools[i].summary;
            card.icon = Icon::Terminal;
            card.selected = i == selected_;
            card.height = 74;
            char id[48];
            std::snprintf(id, sizeof(id), "##utility_%s", tools[i].id);
            if (Card(id, card) == CardResult::Clicked && i != selected_)
            {
                selected_ = i;
                status_.clear();
            }
        }
        Caption("Each one is a script from its author's own site, run in PowerShell. Read its source first if you're unsure about it.");
    }

    void UtilitiesPage::DrawDetail()
    {
        const auto tools = catalog::Utilities();
        if (tools.empty())
            return;
        const Utility& tool = tools[std::min(selected_, tools.size() - 1)];
        SectionTitle(tool.name, tool.author);
        Caption(tool.description);
        InfoRow("Runs", tool.elevated ? "As administrator - Windows asks first" : "As you");

        // The exact command, so nothing runs that you haven't seen.
        SectionTitle("Command");
        {
            const float w = ImGui::GetContentRegionAvail().x;
            const float pad = S(12);
            const float h = TextHeight(Type::Mono) + pad * 2;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(w, h));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(at, at + ImVec2(w, h), Col(c::element::filling), S(c::control::choiceRounding));
            dl->AddRect(at, at + ImVec2(w, h), Col(c::element::stroke), S(c::control::choiceRounding));
            PaintClipped(dl, Type::Mono, ImRect(at + ImVec2(pad, 0), at + ImVec2(w - pad, h)), Col(c::text::text_active), tool.command);
        }

        const float half = Split(2);
        if (Button("Run##utility", ImVec2(half, 36), ButtonKind::Accent, Icon::Terminal))
        {
            statusGood_ = platform::RunPowerShell(tool.command, tool.elevated);
            status_ = statusGood_ ? std::string("Started in a PowerShell window. ") + tool.name + " opens its own window when it's ready."
                                  : "Windows didn't start it - the administrator prompt was declined, or PowerShell is missing.";
        }
        ImGui::SameLine();
        if (Button("View source##utility", ImVec2(half, 36), ButtonKind::Normal, Icon::ExternalLink))
            ctx_.host.OpenUrl(tool.source);
        Tip(tool.source);
        if (!status_.empty())
            Note(statusGood_ ? "Started" : "Not started", status_);
    }
}
