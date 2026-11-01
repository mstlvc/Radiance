// Radiance's line icons, drawn as vectors so they stay sharp at every DPI.
// All are designed on a 20x20 grid with a 1.5 unit stroke.
#pragma once

#include "imgui.h"
#include "imgui_internal.h"

namespace radiance::ui
{
    enum class Icon
    {
        None,
        // Navigation
        Display,
        Resolution,
        Monitor,
        Profiles,
        Settings,
        // Actions and status
        Close,
        Minimise,
        Compare,
        Reset,
        Save,
        Trash,
        Copy,
        Paste,
        Check,
        ChevronDown,
        Plus,
        Edit,
        ExternalLink,
        Info,
        Verify,
        Keycap,
        Refresh,
        Game,
        Terminal,
    };

    void DrawIcon(ImDrawList* dl, Icon icon, const ImRect& box, ImU32 colour);
    // The Radiance mark, centred on `centre`, `size` px across. `glow` (0 for none)
    // paints a soft disc behind it.
    void DrawMark(ImDrawList* dl, ImVec2 centre, float size, ImU32 colour, ImU32 glow = 0);
}
