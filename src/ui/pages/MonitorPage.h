// "Monitor": the panel's own settings over DDC/CI (brightness, contrast, low blue
// light, RGB gains, input source) plus per-display software colour.
#pragma once

#include "ui/Page.h"

#include <future>
#include <optional>
#include <vector>

namespace radiance::ui
{
    class MonitorPage final : public Page
    {
    public:
        explicit MonitorPage(PageContext& context);
        ~MonitorPage() override;

        const char* Label() const override { return "Monitor"; }
        const char* Subtitle() const override { return "Your monitor's own settings, without reaching round the back."; }
        Icon NavIcon() const override { return Icon::Monitor; }
        void OnShow() override;
        void Draw(ImVec2 area) override;

    private:
        void BeginProbe();
        void DrawMonitors();
        void DrawControls();
        void DrawInputDialog();
        void Revert(const MonitorCapability& cap);

        std::vector<MonitorCapability> caps_;
        std::future<std::vector<MonitorCapability>> probe_;
        bool probed_ = false;
        int selected_ = 0;

        // An input switch waiting for a yes: it takes this screen's picture away.
        bool openInputDialog_ = false;
        int inputMonitor_ = -1;
        uint8_t inputValue_ = 0;
    };
}
