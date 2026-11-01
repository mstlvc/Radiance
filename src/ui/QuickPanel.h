// The quick colour popup: what the shortcut opens over a game. Saturation, vibrance
// and the scene looks, in a small window that goes back to the tray when it loses focus.
#pragma once

#include "ui/Page.h"

namespace radiance::ui
{
    namespace QuickMetrics
    {
        constexpr float Width = 400;
        constexpr float Height = 500;
        constexpr float Header = 44;
    }

    class QuickPanel
    {
    public:
        explicit QuickPanel(PageContext& context) : ctx_(context) {}

        // Draws into the current (full-window) ImGui window.
        void Draw(ImVec2 size);
        // True when a client-area point should drag the popup: its header, bar the close button.
        bool IsCaption(ImVec2 point, ImVec2 size) const;

    private:
        void ApplyLook(const LookPreset& preset);
        void Reset();

        PageContext& ctx_;
    };
}
