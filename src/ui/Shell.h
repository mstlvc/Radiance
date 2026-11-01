// The window around the pages. The Radiance theme lays it out like the UI source: a
// top bar with the Radiance mark and the page title, a sidebar of square page tiles,
// and the content area over the gradient-and-particles backdrop. The macOS theme lays it
// out like System Settings: traffic lights over a sidebar of labelled rows with coloured
// icon plates, and the page title over the content.
#pragma once

#include "ui/Backdrop.h"
#include "ui/QuickPanel.h"
#include "ui/Page.h"

#include <array>
#include <memory>

namespace radiance::ui
{
    // The window and its chrome, in logical pixels.
    namespace ShellMetrics
    {
        constexpr float Width = 980;
        constexpr float Height = 620;
        constexpr float TopBar = 50;
        constexpr float Sidebar = 100;
        constexpr float Tile = 60;
        constexpr float TilePitch = 80;
        constexpr float ContentLeft = 120;
        constexpr float ContentTop = 70;
        constexpr float ContentInset = 20;  // right and bottom margin
        constexpr float WindowButton = 50;

        // The macOS layout.
        constexpr float MacSidebar = 210;
        constexpr float MacTitleBar = 56;   // the draggable strip along the top
        constexpr float MacRow = 30;
        constexpr float MacRowPitch = 32;
    }

    class Shell
    {
    public:
        explicit Shell(PageContext& context);

        // Draws the whole window for this frame. `drawEdge` paints the window border
        // when the compositor isn't doing it.
        void Draw(ImVec2 windowSize, bool drawEdge, bool animate);

        void Select(PageId page);
        PageId Current() const { return current_; }
        // The window is being shown again; let the page refresh what it shows.
        void NotifyShown() { showPending_ = true; }

        // The window as the small quick colour popup instead of the full app.
        void SetQuick(bool quick) { quick_ = quick; }
        bool Quick() const { return quick_; }

        // True when a client-area point (device px) should drag the window.
        bool IsCaption(ImVec2 point, ImVec2 windowSize) const;

    private:
        void DrawChrome(ImDrawList* dl, ImVec2 size);
        void DrawSidebar(ImVec2 size);
        void DrawMacSidebar(ImDrawList* dl, ImVec2 size);
        void DrawMacTitle(ImDrawList* dl, ImVec2 size);
        bool MacRowButton(PageId id, float y, ImVec2 size);
        ImVec2 ContentOrigin() const;
        void DrawPage(ImVec2 size);
        void DrawKeepDialog();
        void HandleKeys();

        PageContext& ctx_;
        Backdrop backdrop_;
        QuickPanel quickPanel_;
        bool quick_ = false;
        std::array<std::unique_ptr<Page>, static_cast<size_t>(PageId::Count)> pages_;
        PageId current_ = PageId::Display;
        PageId target_ = PageId::Display;  // the page being faded to
        float pageAlpha_ = 0.0f;
        bool showPending_ = true;
    };
}
