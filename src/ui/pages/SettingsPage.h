// "Settings": how Radiance itself behaves - startup, performance, updates,
// diagnostics, and facts about this PC.
#pragma once

#include "ui/Page.h"

#include <string>

namespace radiance::ui
{
    class SettingsPage final : public Page
    {
    public:
        explicit SettingsPage(PageContext& context);

        const char* Label() const override { return "Settings"; }
        const char* Subtitle() const override { return "How Radiance itself behaves."; }
        Icon NavIcon() const override { return Icon::Settings; }
        void OnShow() override;
        void Draw(ImVec2 area) override;

    private:
        void DrawBehaviour();
        void DrawAbout();
        std::string DiagnosticsReport() const;

        bool launchWithWindows_ = false;
        std::string startupNote_;
        std::string diagnosticsNote_;
        std::string hash_;
        bool hashCopied_ = false;
    };
}
