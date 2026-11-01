// The seams between Radiance's UI and the machine. Pages only talk to these
// interfaces, so each feature (NVIDIA vibrance, gamma ramps, resolution
// switching, DDC/CI, game detection) can be implemented and tested on its own.
//
// Colour (services/ColourEngine.cpp), resolution / scaling (services/DisplayService.cpp),
// DDC/CI (services/MonitorControl.cpp) and game detection (services/GameWatcher.cpp);
// services/Services.cpp puts them together.
#pragma once

#include "core/Model.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace radiance
{
    enum class DriverState
    {
        Unknown,
        Available,           // NVIDIA driver vibrance is reachable for this display
        DisplayNotOnNvidia,  // there is an NVIDIA GPU, but not driving this display
        NoNvidiaCard,
    };

    struct Outcome
    {
        bool ok = false;
        std::string message;
    };

    // Everything the colour engine puts on screen.
    struct ColourTarget
    {
        ColourLook look;
        EyeCare eyeCare;
        std::vector<MonitorColour> monitors;  // per-display gamma / warmth
        // The driver's vibrance level is only written once the user has chosen one,
        // so starting Radiance never changes what NVIDIA Control Panel set.
        bool driverVibranceChosen = false;
    };

    enum class ColourHealth
    {
        Live,     // everything asked for is on screen
        Limited,  // some of it: Windows softened or refused a stage
        Off,      // nothing could be applied
    };

    class IColourEngine
    {
    public:
        virtual ~IColourEngine() = default;

        virtual void Apply(const ColourTarget& target) = 0;
        // While held, show the untouched picture so the look can be judged.
        virtual void SetCompare(bool showNeutral) = 0;
        virtual bool Comparing() const = 0;
        // Puts the look back if a game, a mode change or another app reset it.
        virtual void Reassert() = 0;
        // Gives the screen back exactly as Radiance found it (on exit).
        virtual void HandBack() = 0;

        virtual ColourHealth Health() const = 0;
        virtual DriverState Driver() const = 0;
        // The driver's vibrance level as Radiance found it, and the driver's default.
        virtual std::optional<int> DriverLevelAtStart() const = 0;
        virtual int DriverDefault() const = 0;
        // One line describing where colour is being applied, for the diagnostics report.
        virtual std::string StatusLine() const = 0;
    };

    // A display change that is showing but not kept yet. Nobody may be able to see the
    // screen, so it goes back by itself when the countdown runs out.
    struct PendingChange
    {
        std::string description;  // "1440 x 1080 at 144 Hz", "Full screen scaling"
        int secondsLeft = 0;
    };

    class IDisplayService
    {
    public:
        virtual ~IDisplayService() = default;

        virtual std::vector<DisplayInfo> Displays() = 0;
        virtual std::vector<DisplayMode> Modes(const std::string& device) = 0;
        virtual std::optional<DisplayMode> Current(const std::string& device) = 0;
        // The monitor's native size (refresh 0 when unknown).
        virtual std::optional<DisplayMode> Native(const std::string& device) = 0;

        // Switches now, for this session; Keep() makes it stick, Revert() or the countdown
        // undoes it. Refresh 0 picks the highest. `scaling` is the user's choice of how a
        // lower resolution fills the screen, put on the new mode.
        virtual Outcome Apply(const std::string& device, const DisplayMode& mode, std::optional<ScalingMode> scaling) = 0;

        // How the display is being scaled; nullopt when nothing is (the native mode) or
        // Windows doesn't say.
        virtual std::optional<ScalingMode> Scaling(const std::string& device) = 0;
        // Every display showing a lower resolution, with the same countdown.
        virtual Outcome SetScaling(ScalingMode mode) = 0;
        // Asked of the NVIDIA driver for the main display as it is now (changes nothing).
        virtual bool IntegerScalingAvailable() = 0;
        // True when a mode the main display doesn't list can be made by the NVIDIA driver.
        virtual bool CustomResolutionsAvailable() = 0;

        virtual std::optional<PendingChange> Pending() const = 0;
        virtual void Keep() = 0;
        virtual void Revert() = 0;
        // Reverts a pending change whose countdown has run out. Call often.
        virtual void Tick() = 0;

        // A game's launch resolution: switched without the countdown (the game is the
        // yes) and only for the session, until EndGameSwitch() puts the display back.
        virtual Outcome SwitchForGame(const std::string& device, const DisplayMode& mode, std::optional<ScalingMode> scaling) = 0;
        virtual void EndGameSwitch() = 0;
        // Puts a display back on the mode saved in Windows, after Radiance was interrupted
        // in the middle of a game switch.
        virtual void ReturnToSaved(const std::string& device) = 0;
    };

    class IMonitorControl
    {
    public:
        virtual ~IMonitorControl() = default;

        // Asks every monitor what it supports over DDC/CI. Slow (up to a few seconds per
        // panel): call it off the UI thread.
        virtual std::vector<MonitorCapability> Probe() = 0;
        // Queues a VCP write; it is sent in the background, and while a slider moves
        // only the latest value per control goes out. Returns at once.
        virtual void Write(int monitorIndex, uint8_t vcpCode, int value) = 0;
        // Why the last writes didn't land, "" when they did.
        virtual std::string LastProblem() const = 0;
    };

    struct GameEvent
    {
        bool opened = false;  // false: closed
        const SupportedGame* game = nullptr;
    };

    class IGameWatcher
    {
    public:
        virtual ~IGameWatcher() = default;

        // Looks at the running programs; returns the supported games that opened or
        // closed since the last look. Cheap enough for a timer on the UI thread.
        virtual std::vector<GameEvent> Poll() = 0;
        // A supported game running at the last look, if any (the first one opened).
        virtual const SupportedGame* Running() const = 0;
    };

    struct Services
    {
        std::unique_ptr<IColourEngine> colour;
        std::unique_ptr<IDisplayService> displays;
        std::unique_ptr<IMonitorControl> monitors;
        std::unique_ptr<IGameWatcher> games;
    };

    Services MakeServices();

    std::unique_ptr<IColourEngine> MakeColourEngine();
    std::unique_ptr<IDisplayService> MakeDisplayService();
    std::unique_ptr<IMonitorControl> MakeMonitorControl();
    std::unique_ptr<IGameWatcher> MakeGameWatcher();
}
