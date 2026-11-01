#include "platform/DisplayColour.h"

#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdlib>
#include <cstring>

namespace radiance::platform
{
    namespace
    {
        constexpr double kSofteningSteps[] = { 0.5, 0.25, 0.125 };

        // A real ramp climbs (a little slack allowed) and spans a good part of the range.
        bool LooksLikeARamp(const colour::Ramp& ramp)
        {
            for (int c = 0; c < 3; ++c)
            {
                for (int i = 1; i < 256; ++i)
                {
                    if (ramp[static_cast<size_t>(c * 256 + i)] + 255 < ramp[static_cast<size_t>(c * 256 + i - 1)])
                        return false;
                }
                if (ramp[static_cast<size_t>(c * 256 + 255)] - ramp[static_cast<size_t>(c * 256)] < 16383)
                    return false;
            }
            return true;
        }

        bool Same(const colour::Ramp& a, const colour::Ramp& b)
        {
            // Drivers round what they store; a few steps either way is the same ramp.
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])) > 256)
                    return false;
            }
            return true;
        }
    }

    // ------------------------------------------------------------------ gamma ramps

    std::vector<std::string> GammaRamps::Displays()
    {
        std::vector<std::string> devices;
        DISPLAY_DEVICEW dd{};
        dd.cb = sizeof(dd);
        for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i)
        {
            if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
                devices.push_back(Narrow(dd.DeviceName));
            dd = DISPLAY_DEVICEW{};
            dd.cb = sizeof(dd);
        }
        return devices;
    }

    std::optional<colour::Ramp> GammaRamps::Read(const std::string& device)
    {
        const std::wstring name = Widen(device);
        HDC dc = CreateDCW(nullptr, name.c_str(), nullptr, nullptr);
        if (!dc)
            return std::nullopt;
        colour::Ramp ramp{};
        const BOOL ok = GetDeviceGammaRamp(dc, ramp.data());
        DeleteDC(dc);
        if (!ok)
            return std::nullopt;
        return ramp;
    }

    bool GammaRamps::Write(const std::string& device, const colour::Ramp& ramp)
    {
        const std::wstring name = Widen(device);
        HDC dc = CreateDCW(nullptr, name.c_str(), nullptr, nullptr);
        if (!dc)
            return false;
        colour::Ramp copy = ramp;  // SetDeviceGammaRamp takes a non-const pointer
        const BOOL ok = SetDeviceGammaRamp(dc, copy.data());
        DeleteDC(dc);
        return ok != FALSE;
    }

    bool GammaRamps::Apply(const std::string& device, const std::optional<colour::Ramp>& ramp, Result& result)
    {
        DisplayState& state = displays_[device];

        if (!ramp)
        {
            // Neutral: only touch displays Radiance coloured; give them back their own ramp.
            if (!state.coloured)
                return true;
            const colour::Ramp target = state.original.value_or(colour::IdentityRamp());
            if (Write(device, target))
            {
                state.coloured = false;
                state.written.reset();
                return true;
            }
            ++result.refused;
            result.ok = false;
            return false;
        }

        // Already showing exactly this (slider drags land here most frames). Reassert()
        // notices when something else has since reset the display.
        if (state.coloured && state.written && *state.written == *ramp)
            return true;

        if (!state.coloured)
        {
            // First colour on this display: remember what it had (a calibration ramp, say).
            const std::optional<colour::Ramp> current = Read(device);
            state.original = (current && LooksLikeARamp(*current) && !colour::IsIdentity(*current)) ? current : std::nullopt;
            state.coloured = true;
        }

        if (Write(device, *ramp))
        {
            state.written = *ramp;
            return true;
        }
        // Windows refuses ramps that stray far from neutral (unless the gamma range is
        // unlocked in the registry); land as much of the look as it will take.
        for (const double step : kSofteningSteps)
        {
            const colour::Ramp gentler = colour::Toward(*ramp, step);
            if (Write(device, gentler))
            {
                state.written = *ramp;  // what was asked for; the softened ramp is what holds
                state.softened = gentler;
                ++result.softened;
                return true;
            }
        }
        ++result.refused;
        result.ok = false;
        return false;
    }

    void GammaRamps::Forget(const std::string& device)
    {
        const auto it = displays_.find(device);
        if (it != displays_.end())
        {
            it->second.written.reset();
            it->second.softened.reset();
        }
    }

    bool GammaRamps::StillHolds(const std::string& device) const
    {
        const auto it = displays_.find(device);
        if (it == displays_.end() || !it->second.coloured || !it->second.written)
            return true;
        const std::optional<colour::Ramp> current = Read(device);
        const colour::Ramp& expected = it->second.softened ? *it->second.softened : *it->second.written;
        return current && Same(*current, expected);
    }

    void GammaRamps::RestoreAll()
    {
        for (auto& [device, state] : displays_)
        {
            if (state.coloured)
                Write(device, state.original.value_or(colour::IdentityRamp()));
            state.coloured = false;
            state.written.reset();
            state.softened.reset();
        }
    }

    // ------------------------------------------------------------------ colour filter

    namespace
    {
        struct MagColorEffect
        {
            float transform[5][5];
        };
        using MagInitializeFn = BOOL(WINAPI*)();
        using MagUninitializeFn = BOOL(WINAPI*)();
        using MagSetFullscreenColorEffectFn = BOOL(WINAPI*)(MagColorEffect*);
    }

    ScreenColourFilter::~ScreenColourFilter()
    {
        if (initialised_)
        {
            Set(std::nullopt);
            if (uninitialize_)
                reinterpret_cast<MagUninitializeFn>(uninitialize_)();
        }
        if (module_)
            FreeLibrary(static_cast<HMODULE>(module_));
    }

    bool ScreenColourFilter::Available()
    {
        if (tried_)
            return initialised_;
        tried_ = true;
        module_ = LoadLibraryExW(L"Magnification.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module_)
            return false;
        const auto initialize = reinterpret_cast<MagInitializeFn>(GetProcAddress(static_cast<HMODULE>(module_), "MagInitialize"));
        uninitialize_ = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module_), "MagUninitialize"));
        setEffect_ = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module_), "MagSetFullscreenColorEffect"));
        initialised_ = initialize && setEffect_ && initialize();
        return initialised_;
    }

    bool ScreenColourFilter::Set(const std::optional<colour::Matrix>& matrix)
    {
        if (!Available())
            return false;
        if (!matrix && !active_)
            return true;
        MagColorEffect effect{};
        const colour::Matrix m = matrix.value_or(colour::IdentityMatrix());
        std::memcpy(effect.transform, m.data(), sizeof(effect.transform));
        const bool ok = reinterpret_cast<MagSetFullscreenColorEffectFn>(setEffect_)(&effect) != FALSE;
        if (ok)
            active_ = matrix.has_value();
        return ok;
    }
}
