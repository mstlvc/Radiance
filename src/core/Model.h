// Radiance's plain data types. Nothing here talks to Windows or draws anything;
// the UI edits these, the services apply them, and Settings persists them.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace radiance
{
    // ---------------------------------------------------------------- colour

    enum class ColourVision
    {
        Normal,
        GreenWeak,  // deuteranopia
        RedWeak,    // protanopia
        BlueWeak,   // tritanopia
    };

    // Everything the Display page's sliders describe. Percent values use 100 as
    // "the game as it ships"; signed values use 0 as neutral.
    struct ColourLook
    {
        int saturation = 100;     // 0..300
        int vibrance = 50;        // 0..350: the NVIDIA driver's own scale up to 100 (50 is its default), past it continues in software
        int brightness = 100;     // 50..170
        int gamma = 100;          // 50..150 (shown as 0.50..1.50)
        int contrast = 100;       // 50..150
        int temperature = 0;      // -100..100
        int highlights = 0;       // -100..100
        int shadows = 0;          // -100..100
        int blacks = 0;           // -100..100
        int whites = 0;           // -100..100
        int shadowTint = 0;       // -100..100, cool..warm
        int highlightTint = 0;    // -100..100, cool..warm
        int hue = 0;              // -180..180 degrees
        int redGain = 100;        // 50..150
        int greenGain = 100;      // 50..150
        int blueGain = 100;       // 50..150
        ColourVision vision = ColourVision::Normal;

        bool operator==(const ColourLook&) const = default;
        bool IsNeutral() const { return *this == ColourLook{}; }
    };

    // Eye care sits on top of whatever look is active.
    struct EyeCare
    {
        int warmth = 0;  // 0..100
        int dim = 0;     // 0..80

        bool operator==(const EyeCare&) const = default;
    };

    struct LookPreset
    {
        std::string game;  // catalogue game this look belongs to ("" for the user's own)
        std::string name;
        std::string art;   // resource name of the thumbnail, "" for none
        ColourLook look;
    };

    // ---------------------------------------------------------------- input

    // Modifier bits match RegisterHotKey: MOD_ALT=1, MOD_CONTROL=2, MOD_SHIFT=4, MOD_WIN=8.
    struct Hotkey
    {
        uint32_t modifiers = 0;
        uint32_t virtualKey = 0;
        bool enabled = true;

        bool IsSet() const { return enabled && virtualKey != 0; }
        bool operator==(const Hotkey&) const = default;
        std::string Describe() const;  // "Ctrl + Alt + V", or "Not set"
    };

    // ---------------------------------------------------------------- displays

    struct DisplayMode
    {
        int width = 0;
        int height = 0;
        int refreshHz = 0;

        bool operator==(const DisplayMode&) const = default;
        bool SameSize(const DisplayMode& o) const { return width == o.width && height == o.height; }
    };

    struct DisplayInfo
    {
        std::string deviceName;    // \\.\DISPLAY1
        std::string friendlyName;  // the monitor's own name when Windows knows it
        bool primary = false;
        bool hdrActive = false;
    };

    enum class ScalingMode
    {
        AspectRatio,
        NoScaling,
        FullScreen,
        Integer,
    };

    struct PvpResolution
    {
        const char* name;
        int width;
        int height;
        const char* aspect;
        const char* why;
        const char* tradeOff;
        bool needsStretching;
    };

    // ---------------------------------------------------------------- monitor (DDC/CI)

    struct VcpRange
    {
        int minimum = 0;
        int maximum = 100;
        int current = 50;

        int ToPercent(int raw) const;
        int FromPercent(int percent) const;
    };

    // What a monitor answered when asked over DDC/CI.
    struct MonitorCapability
    {
        int index = 0;
        std::string description;
        std::string deviceName;  // the Windows display the panel is attached to
        std::string refusal;     // why it can't be controlled, when it can't

        bool supportsBrightness = false;
        bool supportsContrast = false;
        bool supportsRgbGain = false;  // drives "Low blue light"
        bool supportsRedGain = false;
        bool supportsGreenGain = false;
        bool supportsInputSource = false;

        VcpRange brightness;
        VcpRange contrast;
        VcpRange redGain;
        VcpRange greenGain;
        VcpRange blueGain;
        std::vector<uint8_t> inputSources;
        std::optional<uint8_t> currentInputSource;

        bool AnyControl() const
        {
            return supportsBrightness || supportsContrast || supportsRgbGain || supportsRedGain || supportsGreenGain || supportsInputSource;
        }
    };

    // The values Radiance last wrote to a panel, and what the panel had before,
    // so "Put it back" can restore it. -1 means "never touched".
    struct PanelSettings
    {
        int monitorIndex = 0;
        int brightness = -1;
        int contrast = -1;
        int lowBlue = 0;
        int redGain = -1;
        int greenGain = -1;
        int originalBrightness = -1;
        int originalContrast = -1;
        int originalRed = -1;
        int originalGreen = -1;
        int originalBlue = -1;

        bool HasOriginals() const { return originalBrightness >= 0 || originalContrast >= 0 || originalRed >= 0 || originalGreen >= 0 || originalBlue >= 0; }
    };

    // Software colour for a single display, DDC/CI or not.
    struct MonitorColour
    {
        std::string deviceName;
        int gammaPercent = 100;  // 50..150
        int warmthPercent = 0;   // 0..100
    };

    // ---------------------------------------------------------------- games

    struct SupportedGame
    {
        const char* id;
        const char* displayName;
        const char* processName;  // without .exe
        const char* lookGame;     // catalogue name for its scene presets, nullptr when none
    };

    struct GameProfile
    {
        std::string gameId;
        std::string displayName;
        ColourLook look;
        int64_t lastUpdated = 0;  // unix seconds
    };

    // "When this game launches, switch to WxH, and back when it closes."
    struct MonitorRule
    {
        std::string gameId;
        int width = 0;
        int height = 0;
    };

    // ---------------------------------------------------------------- utilities

    // A Windows tool the Utilities page starts with one click: its author's own
    // PowerShell one-liner, shown in full before it runs.
    struct Utility
    {
        const char* id;
        const char* name;
        const char* author;
        const char* summary;      // the card's line
        const char* description;  // the detail panel
        const char* command;      // PowerShell
        const char* source;       // where to read the script first
        bool elevated;            // Windows asks for administrator rights
    };
}
