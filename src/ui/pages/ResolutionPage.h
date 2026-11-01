// "Resolution": the display's mode and refresh rate, custom modes, PvP presets,
// scaling, HDR and per-game launch resolutions.
#pragma once

#include "ui/Page.h"

#include <optional>
#include <string>
#include <vector>

namespace radiance::ui
{
    class ResolutionPage final : public Page
    {
    public:
        explicit ResolutionPage(PageContext& context);

        const char* Label() const override { return "Resolution"; }
        const char* Subtitle() const override { return "Change resolution and refresh rate without leaving Radiance."; }
        Icon NavIcon() const override { return Icon::Resolution; }
        void OnShow() override;
        void Draw(ImVec2 area) override;

    private:
        void Refresh();
        void Apply(const DisplayMode& mode);
        void ShowDisplay(int index);
        std::string Device() const;
        std::optional<DisplayMode> Native() const;
        std::vector<DisplayMode> BestPerResolution(size_t limit) const;
        std::vector<int> RatesFor(int width, int height) const;

        void DrawModes();
        void DrawPresetsAndRules();
        void DrawNow();

        std::vector<DisplayInfo> displays_;
        int displayIndex_ = 0;
        std::vector<DisplayMode> modes_;
        std::optional<DisplayMode> current_;
        std::optional<DisplayMode> native_;
        bool integerScaling_ = false;     // what the driver offers, asked on each refresh
        bool customResolutions_ = false;
        double refreshedAt_ = -1.0;

        char customWidth_[8] = {};
        char customHeight_[8] = {};
        char customRate_[8] = {};
        std::string status_;
        bool statusGood_ = true;
    };
}
