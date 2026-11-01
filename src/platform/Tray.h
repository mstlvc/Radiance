// Radiance's notification-area icon and its menu.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace radiance::platform
{
    struct TrayMenuItem
    {
        UINT id = 0;          // 0 draws a separator
        std::wstring text;
        bool enabled = true;
        bool isDefault = false;
    };

    class TrayIcon
    {
    public:
        bool Create(HWND owner, UINT callbackMessage, HICON icon, const std::wstring& tip);
        void Destroy();

        // Explorer restarts drop every tray icon; call this on "TaskbarCreated".
        void Restore();
        void SetTip(const std::wstring& tip);
        void SetIcon(HICON icon);
        // A Windows notification from the icon. Clicking it sends NIN_BALLOONUSERCLICK.
        void Notify(const std::wstring& title, const std::wstring& text);

        // Shows the menu at the cursor and returns the chosen id, or 0.
        UINT ShowMenu(const std::vector<TrayMenuItem>& items);

        static UINT TaskbarCreatedMessage();

    private:
        NOTIFYICONDATAW data_{};
        bool added_ = false;
    };

    // Asks Windows to draw this process's menus dark (Windows 10 1903+). Uses the
    // uxtheme entry points every dark-mode Win32 app relies on; harmless no-op when
    // they are missing.
    void PreferDarkMenus();
}
