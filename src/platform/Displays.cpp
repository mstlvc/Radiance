#include "platform/Displays.h"

#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <set>
#include <tuple>

namespace radiance::platform
{
    namespace
    {
        struct ActivePaths
        {
            std::vector<DISPLAYCONFIG_PATH_INFO> paths;
            std::vector<DISPLAYCONFIG_MODE_INFO> modes;
        };

        bool QueryActive(ActivePaths& out)
        {
            // The configuration can change between sizing and querying; try again then.
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                UINT32 pathCount = 0, modeCount = 0;
                if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
                    return false;
                out.paths.resize(pathCount);
                out.modes.resize(modeCount);
                const LONG result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, out.paths.data(), &modeCount, out.modes.data(), nullptr);
                if (result == ERROR_INSUFFICIENT_BUFFER)
                    continue;
                if (result != ERROR_SUCCESS)
                    return false;
                out.paths.resize(pathCount);
                out.modes.resize(modeCount);
                return true;
            }
            return false;
        }

        std::wstring SourceName(const DISPLAYCONFIG_PATH_INFO& path)
        {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = path.sourceInfo.adapterId;
            source.header.id = path.sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
                return {};
            return source.viewGdiDeviceName;
        }

        std::string MonitorName(const DISPLAYCONFIG_PATH_INFO& path)
        {
            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            target.header.size = sizeof(target);
            target.header.adapterId = path.targetInfo.adapterId;
            target.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS || !target.monitorFriendlyDeviceName[0])
                return {};
            return Narrow(target.monitorFriendlyDeviceName);
        }

        bool HdrOn(const DISPLAYCONFIG_PATH_INFO& path)
        {
            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO info{};
            info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
            info.header.size = sizeof(info);
            info.header.adapterId = path.targetInfo.adapterId;
            info.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&info.header) != ERROR_SUCCESS)
                return false;
            // Advanced colour with wide colour *enforced* is Windows 11's automatic colour
            // management on an SDR screen, not HDR.
            return info.advancedColorEnabled && !info.wideColorEnforced;
        }

        const DISPLAYCONFIG_PATH_INFO* PathFor(const ActivePaths& active, const std::string& device)
        {
            const std::wstring name = Widen(device);
            for (const DISPLAYCONFIG_PATH_INFO& path : active.paths)
            {
                if (_wcsicmp(SourceName(path).c_str(), name.c_str()) == 0)
                    return &path;
            }
            return nullptr;
        }

        PathScaling FromWindows(DISPLAYCONFIG_SCALING scaling)
        {
            switch (scaling)
            {
            case DISPLAYCONFIG_SCALING_IDENTITY: return PathScaling::Identity;
            case DISPLAYCONFIG_SCALING_CENTERED: return PathScaling::Centered;
            case DISPLAYCONFIG_SCALING_STRETCHED: return PathScaling::Stretched;
            case DISPLAYCONFIG_SCALING_ASPECTRATIOCENTEREDMAX: return PathScaling::AspectRatio;
            default: return PathScaling::Other;
            }
        }

        DISPLAYCONFIG_SCALING ToWindows(PathScaling scaling)
        {
            switch (scaling)
            {
            case PathScaling::Identity: return DISPLAYCONFIG_SCALING_IDENTITY;
            case PathScaling::Centered: return DISPLAYCONFIG_SCALING_CENTERED;
            case PathScaling::Stretched: return DISPLAYCONFIG_SCALING_STRETCHED;
            case PathScaling::AspectRatio: return DISPLAYCONFIG_SCALING_ASPECTRATIOCENTEREDMAX;
            default: return DISPLAYCONFIG_SCALING_PREFERRED;
            }
        }

        bool CurrentDevMode(const std::wstring& name, DEVMODEW& dm)
        {
            dm = DEVMODEW{};
            dm.dmSize = sizeof(dm);
            return EnumDisplaySettingsExW(name.c_str(), ENUM_CURRENT_SETTINGS, &dm, 0) != FALSE;
        }
    }

    std::vector<DisplayInfo> AttachedDisplays()
    {
        ActivePaths active;
        const bool havePaths = QueryActive(active);

        std::vector<DisplayInfo> displays;
        DISPLAY_DEVICEW adapter{};
        adapter.cb = sizeof(adapter);
        for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &adapter, 0); ++i)
        {
            if (adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            {
                DisplayInfo info;
                info.deviceName = Narrow(adapter.DeviceName);
                info.primary = (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
                if (const DISPLAYCONFIG_PATH_INFO* path = havePaths ? PathFor(active, info.deviceName) : nullptr)
                {
                    info.friendlyName = MonitorName(*path);
                    info.hdrActive = HdrOn(*path);
                }
                if (info.friendlyName.empty())
                {
                    DISPLAY_DEVICEW monitor{};
                    monitor.cb = sizeof(monitor);
                    if (EnumDisplayDevicesW(adapter.DeviceName, 0, &monitor, 0))
                        info.friendlyName = Narrow(monitor.DeviceString);
                }
                if (info.friendlyName.empty())
                    info.friendlyName = "Display " + std::to_string(displays.size() + 1);
                displays.push_back(std::move(info));
            }
            adapter = DISPLAY_DEVICEW{};
            adapter.cb = sizeof(adapter);
        }
        std::stable_partition(displays.begin(), displays.end(), [](const DisplayInfo& d) { return d.primary; });
        return displays;
    }

    std::vector<DisplayMode> ModesOf(const std::string& device)
    {
        const std::wstring name = Widen(device);
        std::set<std::tuple<int, int, int>> seen;
        std::vector<DisplayMode> modes;
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        for (DWORD i = 0; EnumDisplaySettingsExW(name.empty() ? nullptr : name.c_str(), i, &dm, 0); ++i)
        {
            const bool usable = dm.dmBitsPerPel >= 24 && dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480 &&
                dm.dmDisplayFrequency > 1 && !(dm.dmDisplayFlags & DM_INTERLACED);
            if (usable)
            {
                const auto key = std::make_tuple(static_cast<int>(dm.dmPelsWidth), static_cast<int>(dm.dmPelsHeight), static_cast<int>(dm.dmDisplayFrequency));
                if (seen.insert(key).second)
                    modes.push_back(DisplayMode{ std::get<0>(key), std::get<1>(key), std::get<2>(key) });
            }
            dm = DEVMODEW{};
            dm.dmSize = sizeof(dm);
        }
        return modes;
    }

    std::optional<DisplayMode> CurrentModeOf(const std::string& device)
    {
        DEVMODEW dm;
        if (!CurrentDevMode(Widen(device), dm))
            return std::nullopt;
        return DisplayMode{ static_cast<int>(dm.dmPelsWidth), static_cast<int>(dm.dmPelsHeight), static_cast<int>(dm.dmDisplayFrequency) };
    }

    std::optional<DisplayMode> SavedModeOf(const std::string& device)
    {
        const std::wstring name = Widen(device);
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsExW(name.c_str(), ENUM_REGISTRY_SETTINGS, &dm, 0))
            return std::nullopt;
        return DisplayMode{ static_cast<int>(dm.dmPelsWidth), static_cast<int>(dm.dmPelsHeight), static_cast<int>(dm.dmDisplayFrequency) };
    }

    std::optional<DisplayMode> NativeModeOf(const std::string& device)
    {
        ActivePaths active;
        if (QueryActive(active))
        {
            if (const DISPLAYCONFIG_PATH_INFO* path = PathFor(active, device))
            {
                DISPLAYCONFIG_TARGET_PREFERRED_MODE preferred{};
                preferred.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
                preferred.header.size = sizeof(preferred);
                preferred.header.adapterId = path->targetInfo.adapterId;
                preferred.header.id = path->targetInfo.id;
                if (DisplayConfigGetDeviceInfo(&preferred.header) == ERROR_SUCCESS && preferred.width >= 640 && preferred.height >= 480)
                {
                    const DISPLAYCONFIG_RATIONAL& v = preferred.targetMode.targetVideoSignalInfo.vSyncFreq;
                    const int hz = v.Denominator ? static_cast<int>((v.Numerator + v.Denominator / 2) / v.Denominator) : 0;
                    return DisplayMode{ static_cast<int>(preferred.width), static_cast<int>(preferred.height), hz };
                }
            }
        }
        // No preferred mode reported: the largest one the display offers.
        std::optional<DisplayMode> best;
        for (const DisplayMode& m : ModesOf(device))
        {
            if (!best || static_cast<long long>(m.width) * m.height > static_cast<long long>(best->width) * best->height)
                best = DisplayMode{ m.width, m.height, 0 };
        }
        return best;
    }

    ModeChange SwitchMode(const std::string& device, const DisplayMode& mode)
    {
        const std::wstring name = Widen(device);
        DEVMODEW dm;
        if (!CurrentDevMode(name, dm))
            return ModeChange::Refused;
        dm.dmPelsWidth = static_cast<DWORD>(mode.width);
        dm.dmPelsHeight = static_cast<DWORD>(mode.height);
        dm.dmDisplayFrequency = static_cast<DWORD>(mode.refreshHz);
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

        const auto verdict = [](LONG result) {
            switch (result)
            {
            case DISP_CHANGE_SUCCESSFUL: return ModeChange::Done;
            case DISP_CHANGE_RESTART: return ModeChange::NeedsRestart;
            case DISP_CHANGE_BADMODE: return ModeChange::BadMode;
            default: return ModeChange::Refused;
            }
        };
        const ModeChange test = verdict(ChangeDisplaySettingsExW(name.c_str(), &dm, nullptr, CDS_TEST, nullptr));
        if (test != ModeChange::Done)
            return test;
        return verdict(ChangeDisplaySettingsExW(name.c_str(), &dm, nullptr, 0, nullptr));
    }

    bool SwitchBack(const std::string& device)
    {
        const std::wstring name = Widen(device);
        return ChangeDisplaySettingsExW(name.c_str(), nullptr, nullptr, 0, nullptr) == DISP_CHANGE_SUCCESSFUL;
    }

    bool SaveMode(const std::string& device, const DisplayMode& mode)
    {
        const std::wstring name = Widen(device);
        DEVMODEW dm;
        if (!CurrentDevMode(name, dm))
            return false;
        dm.dmPelsWidth = static_cast<DWORD>(mode.width);
        dm.dmPelsHeight = static_cast<DWORD>(mode.height);
        dm.dmDisplayFrequency = static_cast<DWORD>(mode.refreshHz);
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
        return ChangeDisplaySettingsExW(name.c_str(), &dm, nullptr, CDS_UPDATEREGISTRY | CDS_NORESET, nullptr) == DISP_CHANGE_SUCCESSFUL;
    }

    PathScaling ScalingOf(const std::string& device)
    {
        ActivePaths active;
        if (!QueryActive(active))
            return PathScaling::Unknown;
        const DISPLAYCONFIG_PATH_INFO* path = PathFor(active, device);
        return path ? FromWindows(path->targetInfo.scaling) : PathScaling::Unknown;
    }

    bool SetScalingOf(const std::string& device, PathScaling scaling, bool save)
    {
        ActivePaths active;
        if (!QueryActive(active))
            return false;
        const std::wstring name = Widen(device);
        bool found = false;
        for (DISPLAYCONFIG_PATH_INFO& path : active.paths)
        {
            if (_wcsicmp(SourceName(path).c_str(), name.c_str()) == 0)
            {
                path.targetInfo.scaling = ToWindows(scaling);
                found = true;
            }
        }
        if (!found)
            return false;
        UINT32 flags = SDC_APPLY | SDC_USE_SUPPLIED_DISPLAY_CONFIG | SDC_ALLOW_CHANGES;
        if (save)
            flags |= SDC_SAVE_TO_DATABASE;
        return SetDisplayConfig(static_cast<UINT32>(active.paths.size()), active.paths.data(), static_cast<UINT32>(active.modes.size()), active.modes.data(), flags) == ERROR_SUCCESS;
    }
}
