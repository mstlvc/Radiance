#include "platform/Tray.h"

#include <shellapi.h>

namespace radiance::platform
{
    bool TrayIcon::Create(HWND owner, UINT callbackMessage, HICON icon, const std::wstring& tip)
    {
        data_ = NOTIFYICONDATAW{};
        data_.cbSize = sizeof(data_);
        data_.hWnd = owner;
        data_.uID = 1;
        data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data_.uCallbackMessage = callbackMessage;
        data_.hIcon = icon;
        wcsncpy_s(data_.szTip, tip.c_str(), _TRUNCATE);
        added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
        if (added_)
        {
            data_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &data_);
        }
        return added_;
    }

    void TrayIcon::Destroy()
    {
        if (added_)
            Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }

    void TrayIcon::Restore()
    {
        added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
        if (added_)
            Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }

    void TrayIcon::SetTip(const std::wstring& tip)
    {
        wcsncpy_s(data_.szTip, tip.c_str(), _TRUNCATE);
        if (added_)
            Shell_NotifyIconW(NIM_MODIFY, &data_);
    }

    void TrayIcon::SetIcon(HICON icon)
    {
        data_.hIcon = icon;
        if (!added_)
            return;
        NOTIFYICONDATAW update = data_;
        update.uFlags = NIF_ICON;
        Shell_NotifyIconW(NIM_MODIFY, &update);
    }

    void TrayIcon::Notify(const std::wstring& title, const std::wstring& text)
    {
        if (!added_)
            return;
        NOTIFYICONDATAW note = data_;
        note.uFlags = NIF_INFO;
        wcsncpy_s(note.szInfoTitle, title.c_str(), _TRUNCATE);
        wcsncpy_s(note.szInfo, text.c_str(), _TRUNCATE);
        note.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
        Shell_NotifyIconW(NIM_MODIFY, &note);
    }

    UINT TrayIcon::ShowMenu(const std::vector<TrayMenuItem>& items)
    {
        HMENU menu = CreatePopupMenu();
        if (!menu)
            return 0;
        for (const TrayMenuItem& item : items)
        {
            if (item.id == 0)
            {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                continue;
            }
            AppendMenuW(menu, MF_STRING | (item.enabled ? MF_ENABLED : MF_GRAYED), item.id, item.text.c_str());
            if (item.isDefault)
                SetMenuDefaultItem(menu, item.id, FALSE);
        }
        POINT at{};
        GetCursorPos(&at);
        // The owner must be foreground or the menu won't close when clicking elsewhere.
        SetForegroundWindow(data_.hWnd);
        const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, at.x, at.y, data_.hWnd, nullptr));
        PostMessageW(data_.hWnd, WM_NULL, 0, 0);
        DestroyMenu(menu);
        return chosen;
    }

    UINT TrayIcon::TaskbarCreatedMessage()
    {
        static const UINT message = RegisterWindowMessageW(L"TaskbarCreated");
        return message;
    }

    void PreferDarkMenus()
    {
        HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!uxtheme)
            return;
        // Ordinal 135: SetPreferredAppMode(AppMode), 136: FlushMenuThemes().
        using SetPreferredAppMode = int(WINAPI*)(int);
        using FlushMenuThemes = void(WINAPI*)();
        const auto setMode = reinterpret_cast<SetPreferredAppMode>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
        const auto flush = reinterpret_cast<FlushMenuThemes>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
        if (setMode)
            setMode(2 /* ForceDark */);
        if (flush)
            flush();
    }
}
