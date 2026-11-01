// "Display": saturation and NVIDIA digital vibrance, fine tune, colour balance and
// eye care on the left; scene looks, colour vision, share codes and shortcuts on the right.
#pragma once

#include "ui/Page.h"

#include <string>
#include <vector>

namespace radiance::ui
{
    class DisplayPage final : public Page
    {
    public:
        explicit DisplayPage(PageContext& context);

        const char* Label() const override { return "Display"; }
        const char* Subtitle() const override { return "Scenes are starting points. The sliders work whatever you play."; }
        Icon NavIcon() const override { return Icon::Display; }
        void OnShow() override;
        void Draw(ImVec2 area) override;

    private:
        struct LookEntry
        {
            LookPreset preset;
            bool custom = false;
            int customIndex = -1;
        };

        void DrawColour();
        void DrawLooks();
        void DrawDialogs();

        void RebuildLooks();
        void ApplyLook(const LookPreset& preset);
        void Changed(bool byHand);
        void ResetAll();
        void SaveCustomLook(const std::string& name);

        std::vector<std::string> games_;
        int gameIndex_ = 0;
        std::vector<LookEntry> looks_;

        char nameBuffer_[64] = {};
        char codeBuffer_[64] = {};
        std::string shareStatus_;
        bool shareStatusGood_ = true;
        std::string hotkeyProblem_;
    };
}
