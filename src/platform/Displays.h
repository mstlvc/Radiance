// Displays, their modes and Windows' scaling for each, through the Win32 mode
// APIs (EnumDisplaySettings / ChangeDisplaySettingsEx) and the display-config
// (CCD) APIs. Plain calls, no state: the display service decides when to use them.
#pragma once

#include "core/Model.h"

#include <optional>
#include <string>
#include <vector>

namespace radiance::platform
{
    // Displays attached to the desktop, primary first, with the monitor's own name and HDR state.
    std::vector<DisplayInfo> AttachedDisplays();

    // Progressive 24/32-bit modes, one per size and refresh rate.
    std::vector<DisplayMode> ModesOf(const std::string& device);
    std::optional<DisplayMode> CurrentModeOf(const std::string& device);
    // The mode saved in the registry, which a restart (or SwitchBack) returns to.
    std::optional<DisplayMode> SavedModeOf(const std::string& device);
    // The monitor's preferred (native) size; refresh rate 0 when unknown.
    std::optional<DisplayMode> NativeModeOf(const std::string& device);

    enum class ModeChange
    {
        Done,
        NeedsRestart,  // Windows would only apply it after a restart (not applied)
        BadMode,       // the display doesn't offer it
        Refused,       // the driver or Windows said no
    };
    // Switches for this session only: nothing is written to the registry, so a restart
    // (or SwitchBack) brings back the saved mode. Tests the mode first.
    ModeChange SwitchMode(const std::string& device, const DisplayMode& mode);
    // Returns the display to the mode saved in the registry.
    bool SwitchBack(const std::string& device);
    // Saves the mode being shown so it survives a restart. No mode set happens.
    bool SaveMode(const std::string& device, const DisplayMode& mode);

    // Windows' scaling for the path a display is on (DISPLAYCONFIG_SCALING_*).
    enum class PathScaling
    {
        Unknown,
        Identity,     // nothing is scaled (usually: the native mode)
        Centered,
        Stretched,
        AspectRatio,
        Other,
    };
    PathScaling ScalingOf(const std::string& device);
    // `save` writes it to Windows' display database too, so it outlasts the session.
    bool SetScalingOf(const std::string& device, PathScaling scaling, bool save);
}
