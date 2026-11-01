// Radiance's process: settings, services, the tray icon, and the main window
// (created the first time it is shown, so a tray-only start stays light).
#pragma once

#include "core/Settings.h"
#include "platform/Dx11Renderer.h"
#include "platform/System.h"
#include "platform/Textures.h"
#include "platform/Tray.h"
#include "services/Services.h"
#include "ui/Page.h"

#include <filesystem>
#include <memory>

namespace radiance::ui
{
    class Shell;
}

namespace radiance
{
    struct LaunchOptions
    {
        bool startInTray = false;  // --tray, used by the Windows startup entry
        // --isolated: a copy for testing beside the real one. It skips the one-copy check,
        // the startup entry, global shortcuts, game watching and the tray icon, so it can't
        // disturb the copy the user is running.
        bool isolated = false;
    };

    LaunchOptions ParseLaunchOptions(const wchar_t* commandLine);

    class App final : public ui::AppHost
    {
    public:
        static constexpr const wchar_t* kWindowClass = L"RadianceMainWindow";
        static constexpr UINT kShowMessage = WM_APP + 1;   // another copy asked us to show
        static constexpr UINT kTrayMessage = WM_APP + 2;

        App();
        ~App() override;

        int Run(HINSTANCE instance, const LaunchOptions& options);

        // ui::AppHost
        void HideWindow() override;
        void MinimiseWindow() override;
        void ShowPage(ui::PageId page) override;
        void ColourChanged() override;
        void ThemeChanged() override;
        void SettingsChanged() override;
        bool HotkeysChanged() override;
        bool SetLaunchWithWindows(bool enabled) override;
        bool LaunchWithWindows() const override;
        bool CopyText(const std::string& text) override;
        std::string PasteText() override;
        void OpenUrl(const std::string& url) override;
        void OpenSettingsFolder() override;
        std::string Version() const override;
        std::string ExecutablePath() const override;
        const platform::MachineFacts& Machine() const override { return machine_; }

    private:
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        static void CALLBACK ForegroundChanged(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG object, LONG child, DWORD thread, DWORD time);
        LRESULT Handle(UINT message, WPARAM wParam, LPARAM lParam);

        bool CreateMainWindow();
        bool EnsureRenderer();
        void ApplyDpi(UINT dpi);
        void Frame();
        void ShowMainWindow();
        void Quit();
        void ShowTrayMenu();
        void ResetColours();
        void PushColour();
        void StartColourWatch();
        void StopColourWatch();
        void WatchDisplayChange();
        void LookChanged();
        void PollGames();
        void GameOpened(const SupportedGame& game);
        void GameClosed(const SupportedGame& game);
        void ReviewProfile();
        void RecoverFromInterruptedGame();
        void LeaveGames(bool switchDisplayBack);

        bool RegisterHotkeys();
        void UnregisterHotkeys();
        void SyncHotkeys();
        void ToggleQuick();
        void ShowQuick();
        void LeaveQuick();
        void SetQuickMode(bool quick);
        void PlaceQuick();
        void RememberQuickPosition();
        void TakeScreenshot();
        void ApplyWindowEdge();
        HICON TrayIconForTaskbar() const;
        void PlaceWindow();
        void RememberWindowPosition();

        HINSTANCE instance_ = nullptr;
        HWND hwnd_ = nullptr;
        HICON icon_ = nullptr;
        HICON smallIcon_ = nullptr;
        HICON trayIconLight_ = nullptr;  // for a light taskbar
        UINT dpi_ = 96;
        bool running_ = true;
        bool visible_ = false;
        bool active_ = false;
        bool rendererReady_ = false;
        bool fontsDirty_ = false;
        bool cornersByCompositor_ = false;
        int appliedVibrance_ = 0;
        std::string gameSwitchFor_;  // the game whose launch resolution is showing
        bool hotkeysRegistered_ = false;
        bool isolated_ = false;
        bool quick_ = false;         // the window is the quick colour popup
        ULONGLONG quickShownAt_ = 0;
        std::filesystem::path screenshotFolder_;
        HWINEVENTHOOK foregroundHook_ = nullptr;

        AppSettings settings_;
        std::unique_ptr<SettingsStore> store_;
        Services services_;
        platform::MachineFacts machine_;
        platform::Dx11Renderer renderer_;
        platform::TextureCache textures_;
        platform::TrayIcon tray_;
        std::unique_ptr<ui::PageContext> context_;
        std::unique_ptr<ui::Shell> shell_;
    };
}
