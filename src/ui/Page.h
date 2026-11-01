// What a page is, and what it may ask of the app around it.
#pragma once

#include "core/Settings.h"
#include "platform/System.h"
#include "platform/Textures.h"
#include "services/Services.h"
#include "ui/Icons.h"

#include "imgui.h"

#include <functional>
#include <string>

namespace radiance::ui
{
    enum class PageId
    {
        Display,
        Resolution,
        Monitor,
        Profiles,
        Settings,
        Utilities,  // after Settings so saved page numbers keep their meaning
        Count
    };

    // The app-level actions pages can trigger. Implemented by radiance::App.
    class AppHost
    {
    public:
        virtual ~AppHost() = default;

        virtual void HideWindow() = 0;
        virtual void MinimiseWindow() = 0;
        virtual void ShowPage(PageId page) = 0;

        // Pushes settings.look and settings.eyeCare to the colour engine and saves soon.
        virtual void ColourChanged() = 0;
        // settings.theme changed: swap the palette, the fonts and the window edge.
        virtual void ThemeChanged() = 0;
        // Anything else in AppSettings changed; saves soon.
        virtual void SettingsChanged() = 0;
        // Re-registers global shortcuts; false when Windows refused one.
        virtual bool HotkeysChanged() = 0;

        virtual bool SetLaunchWithWindows(bool enabled) = 0;
        virtual bool LaunchWithWindows() const = 0;

        virtual bool CopyText(const std::string& text) = 0;
        virtual std::string PasteText() = 0;
        virtual void OpenUrl(const std::string& url) = 0;
        virtual void OpenSettingsFolder() = 0;

        virtual std::string Version() const = 0;
        virtual std::string ExecutablePath() const = 0;
        virtual const platform::MachineFacts& Machine() const = 0;
    };

    struct PageContext
    {
        AppSettings& settings;
        Services& services;
        platform::TextureCache& textures;
        AppHost& host;
    };

    class Page
    {
    public:
        explicit Page(PageContext& context) : ctx_(context) {}
        virtual ~Page() = default;

        virtual const char* Label() const = 0;     // sidebar tooltip and top-bar title
        virtual const char* Subtitle() const = 0;  // under the title
        virtual Icon NavIcon() const = 0;

        // Called when the page becomes the visible one.
        virtual void OnShow() {}
        // Draws into the content area at the cursor, `area` device px in size.
        virtual void Draw(ImVec2 area) = 0;

    protected:
        PageContext& ctx_;
    };

    // The content area's usual arrangement: two panels side by side, the left one
    // `leftShare` of the width. Each panel scrolls on its own.
    void TwoPanels(ImVec2 area, float leftShare, const std::function<void()>& left, const std::function<void()>& right);
}
