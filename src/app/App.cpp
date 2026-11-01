#include "app/App.h"

#include "core/ColourMath.h"
#include "platform/Screenshot.h"
#include "platform/Strings.h"
#include "ui/Fonts.h"
#include "ui/Shell.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "../../res/resource.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifndef RADIANCE_VERSION
#define RADIANCE_VERSION "1.0.0"
#endif

namespace radiance
{
    namespace
    {
        enum TrayCommand : UINT
        {
            kTrayOpen = 1,
            kTrayReset,
            kTrayExit,
        };

        // Games and mode switches reset gamma ramps and vibrance. Radiance checks every few
        // seconds, and once more shortly after the foreground window or the display mode
        // changes, giving the game a moment to finish (PlexusX waited about two seconds too).
        enum Timer : UINT_PTR
        {
            kReassertTimer = 1,
            kSettleTimer,
            kDisplayTimer,  // while a resolution change waits to be kept: its countdown
            kGameTimer,     // looks for supported games opening and closing
        };
        constexpr UINT kReassertEveryMs = 5000;
        constexpr UINT kSettleAfterMs = 1800;
        constexpr UINT kGamePollMs = 3000;

        int64_t NowSeconds()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        App* g_app = nullptr;
        IColourEngine* g_engine = nullptr;

        // A crash must not leave the screen tinted or the driver's vibrance changed.
        LONG WINAPI HandBackOnCrash(EXCEPTION_POINTERS*)
        {
            if (IColourEngine* engine = std::exchange(g_engine, nullptr))
                engine->HandBack();
            return EXCEPTION_CONTINUE_SEARCH;
        }

        enum HotkeyId : int
        {
            kHotkeyQuick = 1,
            kHotkeyOpen,
            kHotkeyScreenshot,
        };

        constexpr DWORD kWindowStyle = WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU;
        constexpr DWORD kWindowExStyle = WS_EX_APPWINDOW;

        int Scaled(float logical, UINT dpi)
        {
            return static_cast<int>(std::lround(logical * dpi / 96.0f));
        }

        bool VisibleOnSomeMonitor(int x, int y, int w, int h)
        {
            const RECT r{ x, y, x + w, y + h };
            // Keep the title bar reachable: its middle must be on a screen.
            const POINT title{ x + w / 2, y + 12 };
            return MonitorFromRect(&r, MONITOR_DEFAULTTONULL) != nullptr && MonitorFromPoint(title, MONITOR_DEFAULTTONULL) != nullptr;
        }
    }

    LaunchOptions ParseLaunchOptions(const wchar_t* commandLine)
    {
        LaunchOptions options;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(commandLine, &argc);
        for (int i = 1; argv && i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"--tray") == 0)
                options.startInTray = true;
            else if (_wcsicmp(argv[i], L"--isolated") == 0)
                options.isolated = true;
        }
        LocalFree(argv);
        return options;
    }

    App::App()
    {
        g_app = this;
    }

    App::~App()
    {
        StopColourWatch();
        g_engine = nullptr;
        g_app = nullptr;
        shell_.reset();
        if (rendererReady_)
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        textures_.Clear();
        renderer_.Destroy();
        tray_.Destroy();
        if (icon_) DestroyIcon(icon_);
        if (smallIcon_) DestroyIcon(smallIcon_);
        if (trayIconLight_) DestroyIcon(trayIconLight_);
    }

    int App::Run(HINSTANCE instance, const LaunchOptions& options)
    {
        instance_ = instance;
        isolated_ = options.isolated;
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        store_ = std::make_unique<SettingsStore>(SettingsStore::DefaultFolder() / L"radiance.json");
        const bool hadSettings = store_->Load(settings_);
        machine_ = platform::MeasureMachine();
        ui::SetTheme(ui::ThemeFromName(settings_.theme), settings_.lightMode);
        services_ = MakeServices();
        context_ = std::make_unique<ui::PageContext>(ui::PageContext{ settings_, services_, textures_, *this });

        platform::PreferDarkMenus();
        if (!CreateMainWindow())
            return 1;

        if (!isolated_)
            tray_.Create(hwnd_, kTrayMessage, TrayIconForTaskbar(), L"Radiance");
        RegisterHotkeys();

        // Radiance lives in the tray and starts with Windows unless told otherwise.
        if (!hadSettings || !settings_.firstRunDone)
            settings_.firstRunDone = true;
        if (!isolated_ && settings_.startWithWindows != platform::Startup::IsEnabled())
            platform::Startup::SetEnabled(settings_.startWithWindows);
        store_->Save(settings_);

        RecoverFromInterruptedGame();

        // Until the user picks a vibrance, the slider shows the driver's own level.
        if (!settings_.vibranceChosen)
            settings_.look.vibrance = services_.colour->DriverLevelAtStart().value_or(services_.colour->DriverDefault());
        appliedVibrance_ = settings_.look.vibrance;

        // Put the user's look back straight away, window or not, and keep it there.
        PushColour();
        StartColourWatch();
        if (!isolated_)
        {
            SetTimer(hwnd_, kGameTimer, kGamePollMs, nullptr);
            PollGames();  // a game may already be running
        }

        if (!options.startInTray)
            ShowMainWindow();

        MSG msg{};
        while (running_)
        {
            if (!visible_)
            {
                // In the tray: sleep until Windows has something for us.
                const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
                if (got <= 0)
                    break;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                continue;
            }
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    running_ = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!running_ || !visible_)
                continue;
            if (IsIconic(hwnd_))
            {
                WaitMessage();
                continue;
            }
            // Behind another window: redraw a few times a second, not every vsync.
            if (!active_)
                MsgWaitForMultipleObjects(0, nullptr, FALSE, 66, QS_ALLINPUT);
            Frame();
            store_->FlushIfDue(settings_);
        }
        store_->FlushIfDue(settings_, true);
        CoUninitialize();
        return 0;
    }

    bool App::CreateMainWindow()
    {
        const int iconSize = GetSystemMetrics(SM_CXICON);
        const int smallSize = GetSystemMetrics(SM_CXSMICON);
        icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_RADIANCE), IMAGE_ICON, iconSize, iconSize, 0));
        smallIcon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_RADIANCE), IMAGE_ICON, smallSize, smallSize, 0));
        trayIconLight_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_RADIANCE_LIGHT), IMAGE_ICON, smallSize, smallSize, 0));

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &App::WindowProc;
        wc.hInstance = instance_;
        wc.hIcon = icon_;
        wc.hIconSm = smallIcon_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&wc))
            return false;

        hwnd_ = CreateWindowExW(kWindowExStyle, kWindowClass, L"Radiance", kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            Scaled(ui::ShellMetrics::Width, 96), Scaled(ui::ShellMetrics::Height, 96), nullptr, nullptr, instance_, this);
        if (!hwnd_)
            return false;

        // Rounded corners and a hairline edge drawn by the compositor (Windows 11),
        // dark system chrome everywhere.
        const int round = 2;  // DWMWCP_ROUND
        cornersByCompositor_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof(round)));
        ApplyWindowEdge();

        dpi_ = GetDpiForWindow(hwnd_);
        PlaceWindow();
        return true;
    }

    void App::PlaceWindow()
    {
        const int w = Scaled(ui::ShellMetrics::Width, dpi_);
        const int h = Scaled(ui::ShellMetrics::Height, dpi_);
        int x = settings_.windowX;
        int y = settings_.windowY;
        if (x == INT_MIN || y == INT_MIN || !VisibleOnSomeMonitor(x, y, w, h))
        {
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi);
            x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
            y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
        }
        SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        // The monitor may have a different DPI from the one the window was created on.
        const UINT dpi = GetDpiForWindow(hwnd_);
        if (dpi != dpi_)
        {
            dpi_ = dpi;
            SetWindowPos(hwnd_, nullptr, 0, 0, Scaled(ui::ShellMetrics::Width, dpi_), Scaled(ui::ShellMetrics::Height, dpi_), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
        }
    }

    void App::RememberWindowPosition()
    {
        if (!hwnd_ || IsIconic(hwnd_))
            return;
        RECT r{};
        GetWindowRect(hwnd_, &r);
        if (settings_.windowX != r.left || settings_.windowY != r.top)
        {
            settings_.windowX = r.left;
            settings_.windowY = r.top;
            store_->SaveSoon();
        }
    }

    bool App::EnsureRenderer()
    {
        if (rendererReady_)
            return true;
        if (!renderer_.Create(hwnd_))
            return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // Ctrl+Tab belongs to Radiance's page switching, not ImGui's window switcher.
        GImGui->ConfigNavWindowingKeyNext = 0;
        GImGui->ConfigNavWindowingKeyPrev = 0;
        ImGui_ImplWin32_Init(hwnd_);
        ImGui_ImplDX11_Init(renderer_.Device(), renderer_.Context());

        textures_.Attach(renderer_.Device());
        ui::SetScale(dpi_ / 96.0f);
        ui::ApplyStyle();
        ui::BuildFonts(*io.Fonts);

        shell_ = std::make_unique<ui::Shell>(*context_);
        rendererReady_ = true;
        return true;
    }

    void App::ApplyDpi(UINT dpi)
    {
        dpi_ = dpi;
        if (!rendererReady_)
            return;
        ui::SetScale(dpi_ / 96.0f);
        fontsDirty_ = true;
    }

    void App::Frame()
    {
        if (!rendererReady_)
            return;
        if (fontsDirty_)
        {
            ImGui_ImplDX11_InvalidateDeviceObjects();
            ui::BuildFonts(*ImGui::GetIO().Fonts);
            ImGui_ImplDX11_CreateDeviceObjects();
            ui::ApplyStyle();
            fontsDirty_ = false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        shell_->Draw(ImVec2(static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top)), !cornersByCompositor_, active_);
        ImGui::Render();
        WatchDisplayChange();
        SyncHotkeys();

        const ImVec4 bg = ui::c::background::filling;
        const float clear[4] = { bg.x, bg.y, bg.z, 1.0f };
        if (renderer_.BeginFrame(clear))
        {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            renderer_.Present(true);
        }
    }

    void App::ShowMainWindow()
    {
        if (!EnsureRenderer())
            return;
        if (quick_)
            LeaveQuick();
        ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(hwnd_);
        visible_ = true;
        shell_->NotifyShown();
    }

    void App::HideWindow()
    {
        services_.colour->SetCompare(false);
        services_.displays->Revert();  // closing the window is not a yes
        if (quick_)
        {
            LeaveQuick();
        }
        else
        {
            RememberWindowPosition();
            ShowWindow(hwnd_, SW_HIDE);
            visible_ = false;
            active_ = false;
        }
        if (!hotkeysRegistered_)
            RegisterHotkeys();  // in case the window closed while a shortcut was being recorded
        store_->FlushIfDue(settings_, true);
    }

    // ------------------------------------------------------------------ the quick popup

    // The shortcut opens the popup from the tray; with the full window open it brings
    // that forward on the Display page instead, where the same sliders are.
    void App::ToggleQuick()
    {
        if (visible_ && quick_)
            HideWindow();
        else if (visible_)
            ShowPage(ui::PageId::Display);
        else
            ShowQuick();
    }

    void App::ShowQuick()
    {
        if (!EnsureRenderer())
            return;
        SetQuickMode(true);
        PlaceQuick();
        quickShownAt_ = GetTickCount64();
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);  // a hotkey press lets us take the foreground
        visible_ = true;
        shell_->NotifyShown();
    }

    // Hides the popup and turns the window back into the full app, ready for next time.
    void App::LeaveQuick()
    {
        RememberQuickPosition();
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
        active_ = false;
        SetQuickMode(false);
        PlaceWindow();
    }

    void App::SetQuickMode(bool quick)
    {
        if (quick_ == quick)
            return;
        quick_ = quick;
        if (shell_)
            shell_->SetQuick(quick);
        // The popup is a tool window over the game: no taskbar button, always on top.
        // Styles only change while the window is hidden, so the taskbar keeps up.
        LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
        ex = quick ? ((ex & ~static_cast<LONG_PTR>(WS_EX_APPWINDOW)) | WS_EX_TOOLWINDOW) : ((ex & ~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW)) | WS_EX_APPWINDOW);
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
        SetWindowPos(hwnd_, quick ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    void App::PlaceQuick()
    {
        int w = Scaled(ui::QuickMetrics::Width, dpi_);
        int h = Scaled(ui::QuickMetrics::Height, dpi_);
        int x = settings_.quickX;
        int y = settings_.quickY;
        if (x == INT_MIN || y == INT_MIN || !VisibleOnSomeMonitor(x, y, w, h))
        {
            // First time: the middle of the screen the mouse is on (where the game is).
            POINT cursor{};
            GetCursorPos(&cursor);
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &mi);
            x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
            y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
        }
        SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        const UINT dpi = GetDpiForWindow(hwnd_);
        if (dpi != dpi_)
        {
            ApplyDpi(dpi);
            SetWindowPos(hwnd_, nullptr, 0, 0, Scaled(ui::QuickMetrics::Width, dpi_), Scaled(ui::QuickMetrics::Height, dpi_), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
        }
    }

    void App::RememberQuickPosition()
    {
        RECT r{};
        if (!IsWindowVisible(hwnd_) || !GetWindowRect(hwnd_, &r))
            return;
        if (settings_.quickX != r.left || settings_.quickY != r.top)
        {
            settings_.quickX = r.left;
            settings_.quickY = r.top;
            store_->SaveSoon();
        }
    }

    // ------------------------------------------------------------------ shortcuts

    bool App::RegisterHotkeys()
    {
        hotkeysRegistered_ = true;
        if (isolated_)
            return true;
        bool ok = true;
        const auto add = [&](int id, const Hotkey& key) {
            if (key.IsSet() && !RegisterHotKey(hwnd_, id, key.modifiers | MOD_NOREPEAT, key.virtualKey))
                ok = false;
        };
        add(kHotkeyQuick, settings_.quickPopup);
        add(kHotkeyOpen, settings_.openMain);
        add(kHotkeyScreenshot, settings_.screenshot);
        return ok;
    }

    void App::UnregisterHotkeys()
    {
        for (int id : { kHotkeyQuick, kHotkeyOpen, kHotkeyScreenshot })
            UnregisterHotKey(hwnd_, id);
        hotkeysRegistered_ = false;
    }

    // A registered shortcut swallows its keys system-wide, so while one is being recorded
    // on the Display page they are all let go, and taken back afterwards.
    void App::SyncHotkeys()
    {
        const bool recording = ui::IsCapturingHotkey();
        if (recording && hotkeysRegistered_)
            UnregisterHotkeys();
        else if (!recording && !hotkeysRegistered_)
            RegisterHotkeys();
    }

    // Windows' capture is taken before the gamma ramps, the colour filter and NVIDIA
    // vibrance, so the look goes onto the pixels here: what was on screen is what's saved.
    void App::TakeScreenshot()
    {
        std::optional<platform::Screenshot> shot = platform::CaptureDesktop();
        if (!shot)
        {
            tray_.Notify(L"Screenshot failed", L"Windows didn't hand over the screen. Try again.");
            return;
        }
        const ColourLook& look = settings_.look;
        const std::optional<colour::Matrix> matrix = colour::ColourMatrix(look, false);  // driver vibrance has no capture: copy it in software
        for (const platform::DisplayArea& area : platform::DisplayAreas())
        {
            int gamma = 100, warmth = 0;
            for (const MonitorColour& m : settings_.monitorColours)
            {
                if (m.deviceName == area.device)
                {
                    gamma = m.gammaPercent;
                    warmth = m.warmthPercent;
                }
            }
            const colour::Ramp ramp = colour::DisplayRamp(look, settings_.eyeCare, gamma, warmth);
            const bool flat = colour::IsIdentity(ramp);
            if (flat && !matrix)
                continue;
            const int x0 = std::max(area.left - shot->left, 0);
            const int x1 = std::min(area.right - shot->left, shot->width);
            const int y0 = std::max(area.top - shot->top, 0);
            const int y1 = std::min(area.bottom - shot->top, shot->height);
            for (int y = y0; y < y1 && x1 > x0; ++y)
                colour::ApplyToPixels(&shot->pixels[static_cast<size_t>(y) * shot->width + x0], static_cast<size_t>(x1 - x0), flat ? nullptr : &ramp, matrix ? &*matrix : nullptr);
        }

        const std::filesystem::path folder = platform::ScreenshotFolder();
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        wchar_t name[64];
        wcsftime(name, 64, L"Radiance-%Y-%m-%d-%H%M%S.png", &local);
        const bool saved = !folder.empty() && platform::SavePng(folder / name, *shot);
        const bool copied = platform::CopyImage(hwnd_, *shot);
        if (saved)
        {
            screenshotFolder_ = folder;
            tray_.Notify(L"Screenshot saved", copied ? L"In Pictures\\Radiance, with your colours, and copied. Click to open the folder."
                                                     : L"In Pictures\\Radiance, with your colours. Click to open the folder.");
        }
        else
        {
            tray_.Notify(copied ? L"Screenshot copied" : L"Screenshot failed",
                copied ? L"Couldn't save it to Pictures, but it's on the clipboard." : L"Couldn't save or copy it. Try again.");
        }
    }

    void App::MinimiseWindow()
    {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }

    void App::ShowPage(ui::PageId page)
    {
        ShowMainWindow();
        if (shell_)
            shell_->Select(page);
    }

    void App::Quit()
    {
        if (visible_ && quick_)
            RememberQuickPosition();
        else if (visible_)
            RememberWindowPosition();
        UnregisterHotkeys();
        StopColourWatch();
        services_.displays->Revert();
        LeaveGames(true);
        services_.colour->HandBack();
        store_->FlushIfDue(settings_, true);
        tray_.Destroy();
        running_ = false;
        DestroyWindow(hwnd_);
    }

    void App::ResetColours()
    {
        const ColourVision vision = settings_.look.vision;
        settings_.look = ColourLook{};
        settings_.look.vibrance = services_.colour->DriverDefault();
        settings_.look.vision = vision;
        settings_.eyeCare = EyeCare{};
        settings_.lookName.clear();
        ColourChanged();
    }

    void App::ShowTrayMenu()
    {
        const UINT chosen = tray_.ShowMenu({
            { kTrayOpen, L"Open Radiance", true, true },
            { 0, L"" },
            { kTrayReset, L"Reset colours" },
            { 0, L"" },
            { kTrayExit, L"Exit" },
        });
        switch (chosen)
        {
        case kTrayOpen: ShowMainWindow(); break;
        case kTrayReset: ResetColours(); break;
        case kTrayExit: Quit(); break;
        default: break;
        }
    }

    void App::PushColour()
    {
        ColourTarget target;
        target.look = settings_.look;
        target.eyeCare = settings_.eyeCare;
        target.monitors = settings_.monitorColours;
        target.driverVibranceChosen = settings_.vibranceChosen;
        services_.colour->Apply(target);
    }

    void App::StartColourWatch()
    {
        g_engine = services_.colour.get();
        SetUnhandledExceptionFilter(&HandBackOnCrash);
        SetTimer(hwnd_, kReassertTimer, kReassertEveryMs, nullptr);
        foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, &App::ForegroundChanged, 0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }

    void App::StopColourWatch()
    {
        if (foregroundHook_)
        {
            UnhookWinEvent(foregroundHook_);
            foregroundHook_ = nullptr;
        }
        if (hwnd_)
        {
            KillTimer(hwnd_, kReassertTimer);
            KillTimer(hwnd_, kSettleTimer);
            KillTimer(hwnd_, kGameTimer);
        }
    }

    // The countdown has to run even when the window can't draw (minimised, or the new
    // mode left the screen black), so it rides on a timer too.
    void App::WatchDisplayChange()
    {
        if (services_.displays->Pending())
            SetTimer(hwnd_, kDisplayTimer, 250, nullptr);
    }

    void CALLBACK App::ForegroundChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD)
    {
        if (g_app && g_app->hwnd_)
            SetTimer(g_app->hwnd_, kSettleTimer, kSettleAfterMs, nullptr);  // restarts if already pending
    }

    // ------------------------------------------------------------------ AppHost

    void App::ColourChanged()
    {
        // While a game's profile is showing, the sliders edit that profile.
        if (!settings_.activeProfile.empty())
        {
            for (GameProfile& p : settings_.profiles)
            {
                if (p.gameId == settings_.activeProfile)
                {
                    p.look = settings_.look;
                    p.lastUpdated = NowSeconds();
                }
            }
        }
        LookChanged();
    }

    void App::ThemeChanged()
    {
        ui::SetTheme(ui::ThemeFromName(settings_.theme), settings_.lightMode);
        ApplyWindowEdge();
        fontsDirty_ = true;  // each theme has its own typeface; rebuilt before the next frame (with the style)
        store_->SaveSoon();
    }

    // The hairline the compositor draws round the window (Windows 11) for the theme and
    // mode, and dark or light system chrome.
    // The periwinkle mark reads on a dark taskbar; a light one gets the deeper version.
    HICON App::TrayIconForTaskbar() const
    {
        DWORD light = 0;
        DWORD size = sizeof(light);
        const bool known = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) == ERROR_SUCCESS;
        return known && light && trayIconLight_ ? trayIconLight_ : smallIcon_;
    }

    void App::ApplyWindowEdge()
    {
        if (!hwnd_)
            return;
        // Dark or light system chrome (the window's shadow and the frame Windows draws).
        const BOOL dark = ui::Light() ? FALSE : TRUE;
        DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
        if (!cornersByCompositor_)
            return;
        COLORREF edge;
        if (ui::Mac())
            edge = ui::Light() ? RGB(206, 206, 206) : RGB(72, 72, 74);
        else
            edge = ui::Light() ? RGB(200, 205, 222) : RGB(31, 35, 49);
        DwmSetWindowAttribute(hwnd_, 34 /* DWMWA_BORDER_COLOR */, &edge, sizeof(edge));
    }

    void App::LookChanged()
    {
        if (settings_.look.vibrance != appliedVibrance_)
        {
            settings_.vibranceChosen = true;
            appliedVibrance_ = settings_.look.vibrance;
        }
        PushColour();
        store_->SaveSoon();
    }

    void App::SettingsChanged()
    {
        store_->SaveSoon();
        ReviewProfile();  // a profile saved, forgotten, or switching turned on or off
    }

    // ------------------------------------------------------------------ games

    void App::PollGames()
    {
        if (isolated_)
            return;
        for (const GameEvent& e : services_.games->Poll())
        {
            if (e.opened)
                GameOpened(*e.game);
            else
                GameClosed(*e.game);
        }
        ReviewProfile();
    }

    void App::GameOpened(const SupportedGame& game)
    {
        // Its launch resolution, on the primary display, before the game draws much.
        const MonitorRule* rule = settings_.monitorRulesEnabled ? settings_.RuleFor(game.id) : nullptr;
        if (rule && gameSwitchFor_.empty())
        {
            const std::vector<DisplayInfo> displays = services_.displays->Displays();
            if (!displays.empty())
            {
                const std::string device = displays.front().deviceName;
                if (services_.displays->SwitchForGame(device, DisplayMode{ rule->width, rule->height, 0 }, settings_.scaling).ok)
                {
                    gameSwitchFor_ = game.id;
                    settings_.gameDisplayDevice = device;
                    store_->SaveSoon();
                }
            }
        }

        // A game Radiance has looks for, but no profile: offer one on the Profiles page.
        const bool declined = std::find(settings_.declinedGameOffers.begin(), settings_.declinedGameOffers.end(), game.id) != settings_.declinedGameOffers.end();
        if (game.lookGame && !settings_.ProfileFor(game.id) && settings_.pendingGameOfferId.empty() && !declined)
        {
            settings_.pendingGameOfferId = game.id;
            store_->SaveSoon();
        }
    }

    void App::GameClosed(const SupportedGame& game)
    {
        if (gameSwitchFor_ == game.id)
        {
            services_.displays->EndGameSwitch();
            gameSwitchFor_.clear();
            settings_.gameDisplayDevice.clear();
            store_->SaveSoon();
        }
    }

    // Puts the right look on screen: the running game's profile when there is one and
    // switching is on, otherwise the user's own look (kept aside while a profile shows).
    void App::ReviewProfile()
    {
        const SupportedGame* running = services_.games->Running();
        std::string wanted;
        if (running && settings_.autoApplyProfiles && settings_.ProfileFor(running->id))
            wanted = running->id;
        if (wanted == settings_.activeProfile)
            return;

        // Colour-vision correction belongs to the person, not the game: it stays put.
        const ColourVision vision = settings_.look.vision;
        if (!settings_.activeProfile.empty())
        {
            if (settings_.lookBeforeGame)
                settings_.look = *settings_.lookBeforeGame;
            settings_.lookBeforeGame.reset();
            settings_.activeProfile.clear();
        }
        if (const GameProfile* profile = wanted.empty() ? nullptr : settings_.ProfileFor(wanted))
        {
            settings_.lookBeforeGame = settings_.look;
            settings_.look = profile->look;
            settings_.activeProfile = wanted;
        }
        settings_.look.vision = vision;
        LookChanged();
    }

    // Radiance stopped in the middle of a game last time (a crash, a power cut): put
    // the user's own look and the saved display mode back before anything else.
    void App::RecoverFromInterruptedGame()
    {
        if (settings_.lookBeforeGame || !settings_.activeProfile.empty() || !settings_.gameDisplayDevice.empty())
            store_->SaveSoon();
        if (settings_.lookBeforeGame)
            settings_.look = *settings_.lookBeforeGame;
        settings_.lookBeforeGame.reset();
        settings_.activeProfile.clear();
        if (!settings_.gameDisplayDevice.empty())
        {
            services_.displays->ReturnToSaved(settings_.gameDisplayDevice);
            settings_.gameDisplayDevice.clear();
        }
    }

    // On the way out: the user's own look goes back into the settings, and (unless
    // Windows is shutting down anyway) the display back to its mode.
    void App::LeaveGames(bool switchDisplayBack)
    {
        if (switchDisplayBack)
            services_.displays->EndGameSwitch();
        gameSwitchFor_.clear();
        settings_.gameDisplayDevice.clear();
        if (settings_.lookBeforeGame)
            settings_.look = *settings_.lookBeforeGame;
        settings_.lookBeforeGame.reset();
        settings_.activeProfile.clear();
    }

    bool App::HotkeysChanged()
    {
        // Global shortcuts are registered by the hotkey service in the next phase.
        store_->SaveSoon();
        return true;
    }

    bool App::SetLaunchWithWindows(bool enabled)
    {
        return platform::Startup::SetEnabled(enabled);
    }

    bool App::LaunchWithWindows() const
    {
        return platform::Startup::IsEnabled();
    }

    bool App::CopyText(const std::string& text)
    {
        return platform::CopyToClipboard(hwnd_, text);
    }

    std::string App::PasteText()
    {
        return platform::ReadClipboard(hwnd_);
    }

    void App::OpenUrl(const std::string& url)
    {
        platform::OpenUrl(url);
    }

    void App::OpenSettingsFolder()
    {
        platform::OpenFolder(store_->File().parent_path().wstring());
    }

    std::string App::Version() const
    {
        return RADIANCE_VERSION;
    }

    std::string App::ExecutablePath() const
    {
        return platform::Narrow(platform::ExecutablePath());
    }

    // ------------------------------------------------------------------ window procedure

    LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* created = static_cast<App*>(cs->lpCreateParams);
            created->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        }
        auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (app && app->hwnd_ == hwnd)
            return app->Handle(message, wParam, lParam);
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT App::Handle(UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (rendererReady_ && ImGui_ImplWin32_WndProcHandler(hwnd_, message, wParam, lParam))
            return TRUE;

        if (message == platform::TrayIcon::TaskbarCreatedMessage())
        {
            tray_.Restore();
            return 0;
        }

        switch (message)
        {
        case WM_NCHITTEST:
        {
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd_, &p);
            RECT rc{};
            GetClientRect(hwnd_, &rc);
            if (shell_ && shell_->IsCaption(ImVec2(static_cast<float>(p.x), static_cast<float>(p.y)), ImVec2(static_cast<float>(rc.right), static_cast<float>(rc.bottom))))
                return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
                renderer_.RequestResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_ACTIVATE:
            active_ = LOWORD(wParam) != WA_INACTIVE;
            // The popup goes when you click back into the game. Not in its first moments,
            // though: when Windows won't hand over the foreground, focus bounces straight back.
            if (!active_ && quick_ && visible_ && GetTickCount64() - quickShownAt_ > 500)
                HideWindow();
            return 0;
        case WM_HOTKEY:
            switch (static_cast<int>(wParam))
            {
            case kHotkeyQuick: ToggleQuick(); break;
            case kHotkeyOpen: ShowMainWindow(); break;
            case kHotkeyScreenshot: TakeScreenshot(); break;
            default: break;
            }
            return 0;
        case WM_DPICHANGED:
        {
            ApplyDpi(HIWORD(wParam));
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            const float w = quick_ ? ui::QuickMetrics::Width : ui::ShellMetrics::Width;
            const float h = quick_ ? ui::QuickMetrics::Height : ui::ShellMetrics::Height;
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, Scaled(w, dpi_), Scaled(h, dpi_), SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE:
            HideWindow();  // closing the window sends Radiance back to the tray
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_CLOSE)
            {
                HideWindow();
                return 0;
            }
            if ((wParam & 0xFFF0) == SC_KEYMENU)
                return 0;
            break;
        case kShowMessage:
            ShowMainWindow();
            return 0;
        case kTrayMessage:
            switch (LOWORD(lParam))
            {
            case NIN_SELECT:
            case NIN_KEYSELECT:
            case WM_LBUTTONDBLCLK:
                ShowMainWindow();
                break;
            case WM_CONTEXTMENU:
                ShowTrayMenu();
                break;
            case NIN_BALLOONUSERCLICK:
                if (!screenshotFolder_.empty())
                    platform::OpenFolder(screenshotFolder_.wstring());
                break;
            default:
                break;
            }
            return 0;
        case WM_TIMER:
            if (wParam == kSettleTimer)
                KillTimer(hwnd_, kSettleTimer);
            if (wParam == kReassertTimer || wParam == kSettleTimer)
            {
                services_.colour->Reassert();
                if (wParam == kSettleTimer)
                    PollGames();  // a new foreground window may be a game that just opened
                store_->FlushIfDue(settings_);  // also while in the tray, where no frames run
                return 0;
            }
            if (wParam == kGameTimer)
            {
                PollGames();
                return 0;
            }
            if (wParam == kDisplayTimer)
            {
                services_.displays->Tick();
                if (!services_.displays->Pending())
                    KillTimer(hwnd_, kDisplayTimer);
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
            SetTimer(hwnd_, kSettleTimer, kSettleAfterMs, nullptr);
            break;
        case WM_SETTINGCHANGE:
            // Windows' light / dark taskbar switch.
            if (lParam && wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0)
                tray_.SetIcon(TrayIconForTaskbar());
            break;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam)
            {
                // Windows is going away: the driver keeps its vibrance level across a
                // restart, so hand it back now.
                StopColourWatch();
                services_.displays->Revert();
                LeaveGames(false);
                services_.colour->HandBack();
                store_->FlushIfDue(settings_, true);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}
