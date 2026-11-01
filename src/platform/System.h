// Small pieces of Windows plumbing: single instance, launch with Windows,
// clipboard and the running executable.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace radiance::platform
{
    // Holds a named mutex for the life of the process.
    class SingleInstance
    {
    public:
        SingleInstance();
        ~SingleInstance();
        SingleInstance(const SingleInstance&) = delete;
        SingleInstance& operator=(const SingleInstance&) = delete;

        bool IsFirst() const { return first_; }

        // Asks the running copy to show its window (it owns a window of this class).
        static void WakeTheOtherCopy(const wchar_t* windowClass, UINT showMessage);

    private:
        HANDLE mutex_ = nullptr;
        bool first_ = true;
    };

    // HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Radiance = "<exe>" --tray
    namespace Startup
    {
        bool IsEnabled();
        bool SetEnabled(bool enabled);
    }

    std::wstring ExecutablePath();
    bool CopyToClipboard(HWND owner, const std::string& utf8);
    std::string ReadClipboard(HWND owner);
    void OpenUrl(const std::string& url);
    // Starts PowerShell running `command` in a window of its own. `elevated` goes through
    // Windows' administrator prompt; false when Windows didn't start it (or the prompt
    // was declined).
    bool RunPowerShell(const std::string& command, bool elevated);
    void OpenFolder(const std::wstring& folder);

    // Lower-case hex SHA-256 of a file, or "" when it can't be read.
    std::string Sha256OfFile(const std::wstring& path);

    // What Radiance measures about the PC when it starts (Settings > This PC).
    struct MachineFacts
    {
        bool elevated = false;
        std::string gpu;          // primary adapter's name
        bool nvidia = false;      // an NVIDIA adapter is present
        int monitors = 0;
        std::string windows;      // "Windows 11 (build 26200)"
    };
    MachineFacts MeasureMachine();
}
