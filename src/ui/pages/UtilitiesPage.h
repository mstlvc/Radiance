// "Utilities": trusted Windows tools started from Radiance with one click. Each is its
// author's own PowerShell one-liner, shown in full next to its Run button.
#pragma once

#include "ui/Page.h"

#include <string>

namespace radiance::ui
{
    class UtilitiesPage final : public Page
    {
    public:
        explicit UtilitiesPage(PageContext& context) : Page(context) {}

        const char* Label() const override { return "Utilities"; }
        const char* Subtitle() const override { return "Trusted Windows tools, one click away."; }
        Icon NavIcon() const override { return Icon::Terminal; }
        void Draw(ImVec2 area) override;

    private:
        void DrawList();
        void DrawDetail();

        size_t selected_ = 0;
        std::string status_;
        bool statusGood_ = true;
    };
}
