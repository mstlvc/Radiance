#include "platform/System.h"

#include "platform/Strings.h"

#include <shellapi.h>
#include <bcrypt.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace radiance::platform
{
    namespace
    {
        constexpr const wchar_t* kMutexName = L"Local\\Radiance.SingleInstance";
        constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        constexpr const wchar_t* kRunValue = L"Radiance";

        std::wstring StartupCommand()
        {
            return L"\"" + ExecutablePath() + L"\" --tray";
        }
    }

    SingleInstance::SingleInstance()
    {
        mutex_ = CreateMutexW(nullptr, TRUE, kMutexName);
        first_ = mutex_ != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
    }

    SingleInstance::~SingleInstance()
    {
        if (mutex_)
        {
            if (first_)
                ReleaseMutex(mutex_);
            CloseHandle(mutex_);
        }
    }

    void SingleInstance::WakeTheOtherCopy(const wchar_t* windowClass, UINT showMessage)
    {
        if (HWND other = FindWindowW(windowClass, nullptr))
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(other, &pid);
            AllowSetForegroundWindow(pid);
            PostMessageW(other, showMessage, 0, 0);
        }
    }

    namespace Startup
    {
        namespace
        {
            bool ReadValue(std::wstring& out)
            {
                wchar_t value[2048];
                DWORD size = sizeof(value);
                if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
                    return false;
                out = value;
                return true;
            }

            // The executable a startup command launches ("C:\x\Radiance.exe" --tray -> C:\x\Radiance.exe).
            std::wstring ExecutableOf(const std::wstring& command)
            {
                if (!command.empty() && command[0] == L'"')
                {
                    const size_t close = command.find(L'"', 1);
                    return command.substr(1, close == std::wstring::npos ? std::wstring::npos : close - 1);
                }
                const size_t space = command.find(L' ');
                return command.substr(0, space);
            }

            // An entry written by another Radiance that still exists on disk is that copy's
            // to manage; one pointing at a file that is gone is fair to replace or remove.
            bool BelongsToAnotherCopy(const std::wstring& command)
            {
                const std::wstring exe = ExecutableOf(command);
                if (exe.empty() || _wcsicmp(exe.c_str(), ExecutablePath().c_str()) == 0)
                    return false;
                return GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES;
            }
        }

        bool IsEnabled()
        {
            std::wstring value;
            // Only count it as ours when it points at this copy of Radiance.
            return ReadValue(value) && _wcsicmp(value.c_str(), StartupCommand().c_str()) == 0;
        }

        bool SetEnabled(bool enabled)
        {
            std::wstring existing;
            const bool hasValue = ReadValue(existing);
            if (hasValue && BelongsToAnotherCopy(existing))
                return false;  // leave another installed copy's startup entry alone
            if (!enabled && !hasValue)
                return true;

            HKEY key = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
                return false;
            LSTATUS status;
            if (enabled)
            {
                const std::wstring command = StartupCommand();
                status = RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
            }
            else
            {
                status = RegDeleteValueW(key, kRunValue);
                if (status == ERROR_FILE_NOT_FOUND)
                    status = ERROR_SUCCESS;
            }
            RegCloseKey(key);
            return status == ERROR_SUCCESS;
        }
    }

    std::wstring ExecutablePath()
    {
        std::vector<wchar_t> buffer(MAX_PATH);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length < buffer.size())
                return std::wstring(buffer.data(), length);
            buffer.resize(buffer.size() * 2);
        }
    }

    bool CopyToClipboard(HWND owner, const std::string& utf8)
    {
        const std::wstring text = Widen(utf8);
        if (!OpenClipboard(owner))
            return false;
        EmptyClipboard();
        bool ok = false;
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t)))
        {
            if (void* target = GlobalLock(memory))
            {
                memcpy(target, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(memory);
                ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!ok)
                GlobalFree(memory);
        }
        CloseClipboard();
        return ok;
    }

    std::string ReadClipboard(HWND owner)
    {
        std::string out;
        if (!OpenClipboard(owner))
            return out;
        if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
        {
            if (const auto* text = static_cast<const wchar_t*>(GlobalLock(data)))
            {
                out = Narrow(text);
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
        return out;
    }

    void OpenUrl(const std::string& url)
    {
        ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    bool RunPowerShell(const std::string& command, bool elevated)
    {
        // -Command takes the rest of the line; the one-liners have no quotes of their own.
        const std::wstring parameters = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + Widen(command) + L"\"";
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOASYNC;
        info.lpVerb = elevated ? L"runas" : L"open";
        info.lpFile = L"powershell.exe";
        info.lpParameters = parameters.c_str();
        info.nShow = SW_SHOWNORMAL;
        return ShellExecuteExW(&info) != FALSE;
    }

    void OpenFolder(const std::wstring& folder)
    {
        CreateDirectoryW(folder.c_str(), nullptr);
        ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    std::string Sha256OfFile(const std::wstring& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
            return {};
        std::string hex;
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0)
        {
            std::vector<char> buffer(1 << 16);
            bool ok = true;
            while (in && ok)
            {
                in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto got = in.gcount();
                if (got > 0)
                    ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(got), 0) == 0;
            }
            UCHAR digest[32];
            if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0)
            {
                char part[3];
                for (UCHAR b : digest)
                {
                    std::snprintf(part, sizeof(part), "%02x", b);
                    hex += part;
                }
            }
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
        return hex;
    }

    MachineFacts MeasureMachine()
    {
        MachineFacts facts;

        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            TOKEN_ELEVATION elevation{};
            DWORD size = 0;
            if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
                facts.elevated = elevation.TokenIsElevated != 0;
            CloseHandle(token);
        }

        Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;
                if (facts.gpu.empty())
                    facts.gpu = Narrow(desc.Description);
                if (desc.VendorId == 0x10DE)
                    facts.nvidia = true;
            }
        }
        if (facts.gpu.empty())
            facts.gpu = "Unknown";

        facts.monitors = GetSystemMetrics(SM_CMONITORS);

        // RtlGetVersion reports the real build even without a compatibility manifest entry.
        using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
        {
            if (const auto getVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")))
            {
                RTL_OSVERSIONINFOW info{};
                info.dwOSVersionInfoSize = sizeof(info);
                if (getVersion(&info) == 0)
                {
                    const bool eleven = info.dwMajorVersion == 10 && info.dwBuildNumber >= 22000;
                    facts.windows = std::string(eleven ? "Windows 11" : "Windows 10") + " (build " + std::to_string(info.dwBuildNumber) + ")";
                }
            }
        }
        return facts;
    }
}
