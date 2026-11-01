// Resolution, refresh rate and scaling. Every change is made for this session only
// and shown with a countdown: Keep() saves it, anything else (Revert(), the countdown
// running out, Radiance closing) puts the display back the way it was. A mode the
// monitor can't show leaves the screen black, so nothing is kept without a yes.
//
// On displays the NVIDIA GPU drives, scaling is the driver's own setting (what NVIDIA
// Control Panel shows, integer scaling included), and a mode the display doesn't list
// can be tried as an NVIDIA custom resolution - saved to the driver only on Keep().
// Elsewhere scaling goes through Windows' display paths.
#include "services/Services.h"

#include "platform/Displays.h"
#include "platform/NvidiaDisplay.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <map>

namespace radiance
{
    namespace
    {
        using platform::PathScaling;
        namespace NvScaling = platform::NvScaling;

        constexpr ULONGLONG kKeepWindowMs = 15000;

        std::string SizeText(const DisplayMode& m)
        {
            return std::to_string(m.width) + " \xC3\x97 " + std::to_string(m.height);
        }

        std::string ModeText(const DisplayMode& m)
        {
            return SizeText(m) + (m.refreshHz > 0 ? " at " + std::to_string(m.refreshHz) + " Hz" : "");
        }

        const char* ScalingName(ScalingMode mode)
        {
            switch (mode)
            {
            case ScalingMode::AspectRatio: return "Aspect ratio";
            case ScalingMode::NoScaling: return "No scaling";
            case ScalingMode::FullScreen: return "Full screen";
            case ScalingMode::Integer: return "Integer";
            }
            return "";
        }

        PathScaling ToPath(ScalingMode mode)
        {
            switch (mode)
            {
            case ScalingMode::AspectRatio: return PathScaling::AspectRatio;
            case ScalingMode::NoScaling: return PathScaling::Centered;
            case ScalingMode::FullScreen: return PathScaling::Stretched;
            default: return PathScaling::Unknown;  // integer scaling is NVIDIA's alone
            }
        }

        std::optional<ScalingMode> FromPath(PathScaling scaling)
        {
            switch (scaling)
            {
            case PathScaling::AspectRatio: return ScalingMode::AspectRatio;
            case PathScaling::Centered: return ScalingMode::NoScaling;
            case PathScaling::Stretched: return ScalingMode::FullScreen;
            default: return std::nullopt;
            }
        }

        int ToNvidia(ScalingMode mode)
        {
            switch (mode)
            {
            case ScalingMode::AspectRatio: return NvScaling::Aspect;
            case ScalingMode::NoScaling: return NvScaling::Centered;
            case ScalingMode::FullScreen: return NvScaling::ToNative;
            case ScalingMode::Integer: return NvScaling::Integer;
            }
            return -1;
        }

        std::optional<ScalingMode> FromNvidia(int scaling)
        {
            switch (scaling)
            {
            case NvScaling::Aspect: return ScalingMode::AspectRatio;
            case NvScaling::Centered: return ScalingMode::NoScaling;
            case NvScaling::ToNative: return ScalingMode::FullScreen;
            case NvScaling::Integer: return ScalingMode::Integer;
            default: return std::nullopt;  // the monitor does the scaling, or a driver default
            }
        }

        // Only these are worth putting back; Identity / Other are Windows' own business.
        bool Restorable(PathScaling scaling)
        {
            return scaling == PathScaling::AspectRatio || scaling == PathScaling::Centered || scaling == PathScaling::Stretched;
        }

        // A display's scaling as found, so it can be put back.
        struct ScalingState
        {
            bool nvidia = false;
            int nv = -1;
            PathScaling path = PathScaling::Unknown;
        };
        using ScalingSnapshot = std::map<std::string, ScalingState>;

        class DisplayService final : public IDisplayService
        {
        public:
            ~DisplayService() override
            {
                Revert();
                EndGameSwitch();
            }

            std::vector<DisplayInfo> Displays() override { return platform::AttachedDisplays(); }
            std::vector<DisplayMode> Modes(const std::string& device) override { return platform::ModesOf(device); }
            std::optional<DisplayMode> Current(const std::string& device) override { return platform::CurrentModeOf(device); }
            std::optional<DisplayMode> Native(const std::string& device) override { return platform::NativeModeOf(device); }

            Outcome Apply(const std::string& device, const DisplayMode& wanted, std::optional<ScalingMode> scaling) override
            {
                Revert();  // one change at a time: anything still waiting to be kept goes back first
                const std::optional<DisplayMode> now = platform::CurrentModeOf(device);
                if (!now)
                    return { false, "Windows didn't report this display's mode, so nothing changed." };
                DisplayMode mode = Resolve(device, wanted);
                if (mode.refreshHz <= 0)
                    mode.refreshHz = now->refreshHz;

                Waiting change{ device, *now, Snapshot() };
                bool changed = false;
                if (!(mode == *now))
                {
                    switch (platform::SwitchMode(device, mode))
                    {
                    case platform::ModeChange::Done:
                        break;
                    case platform::ModeChange::NeedsRestart:
                        return { false, "Windows would only switch to " + ModeText(mode) + " after a restart, so nothing changed." };
                    case platform::ModeChange::BadMode:
                        // Not a mode the display lists: the NVIDIA driver may be able to make it.
                        if (const std::optional<Outcome> custom = TryCustomMode(device, mode, *now, change))
                        {
                            if (!custom->ok)
                                return *custom;
                            break;
                        }
                        return { false, NotOffered(device, mode) };
                    default:
                        return { false, "Windows refused " + ModeText(mode) + ". Nothing changed." };
                    }
                    change.modeAfter = mode;
                    changed = true;
                }
                if (scaling)
                {
                    const Put put = PutScaling(device, *scaling);
                    changed |= put == Put::Shown;
                    change.scalingChanged |= put == Put::Shown || put == Put::Saved;
                }
                if (!changed)
                    return { true, {} };

                change.description = ModeText(mode) + (change.customTrial ? " (a new custom resolution)" : "");
                change.deadline = GetTickCount64() + kKeepWindowMs;
                pending_ = std::move(change);
                return { true, {} };
            }

            std::optional<ScalingMode> Scaling(const std::string& device) override
            {
                if (nv_.Drives(device))
                {
                    const std::optional<int> s = nv_.Scaling(device);
                    return s ? FromNvidia(*s) : std::nullopt;
                }
                return FromPath(platform::ScalingOf(device));
            }

            Outcome SetScaling(ScalingMode mode) override
            {
                Revert();
                if (mode == ScalingMode::Integer && !IntegerScalingAvailable())
                    return { false, "The NVIDIA driver won't use integer scaling on this display right now." };
                Waiting change{ {}, std::nullopt, Snapshot() };
                bool shown = false, saved = false, refused = false;
                for (const DisplayInfo& d : platform::AttachedDisplays())
                {
                    switch (PutScaling(d.deviceName, mode))
                    {
                    case Put::Shown: shown = true; break;
                    case Put::Saved: saved = true; break;
                    case Put::Refused: refused = true; break;
                    default: break;
                    }
                }
                if (shown)
                {
                    change.scalingChanged = true;
                    change.description = std::string(ScalingName(mode)) + " scaling";
                    change.deadline = GetTickCount64() + kKeepWindowMs;
                    pending_ = std::move(change);
                    return { true, {} };
                }
                if (refused && !saved)
                    return { false, std::string("The driver refused ") + ScalingName(mode) + " scaling. Nothing changed." };
                return { true, std::string("Saved. Nothing is scaled at your native resolution; ") + ScalingName(mode) +
                                   " is used whenever a lower one is showing." };
            }

            bool IntegerScalingAvailable() override
            {
                // Asked of the driver without changing anything, once per display and mode.
                const std::vector<DisplayInfo> displays = platform::AttachedDisplays();
                if (displays.empty() || !nv_.Drives(displays.front().deviceName))
                    return false;
                const std::string& device = displays.front().deviceName;
                const std::optional<DisplayMode> now = platform::CurrentModeOf(device);
                const std::string key = device + (now ? ModeText(*now) : std::string());
                if (key != integerKey_)
                {
                    integerKey_ = key;
                    integerOk_ = nv_.Scaling(device) == NvScaling::Integer || nv_.SetScaling(device, NvScaling::Integer, true);
                }
                return integerOk_;
            }

            bool CustomResolutionsAvailable() override
            {
                const std::vector<DisplayInfo> displays = platform::AttachedDisplays();
                return !displays.empty() && nv_.Drives(displays.front().deviceName);
            }

            std::optional<PendingChange> Pending() const override
            {
                if (!pending_)
                    return std::nullopt;
                const ULONGLONG now = GetTickCount64();
                const ULONGLONG left = pending_->deadline > now ? pending_->deadline - now : 0;
                return PendingChange{ pending_->description, static_cast<int>((left + 999) / 1000) };
            }

            void Keep() override
            {
                if (!pending_)
                    return;
                const Waiting change = std::move(*pending_);
                pending_.reset();
                if (change.customTrial)
                    nv_.SaveCustom(change.device);  // the driver adds it to the display's modes
                if (change.modeAfter)
                {
                    platform::SaveMode(change.device, *change.modeAfter);
                    // A mode the user kept outranks a game's: don't undo it when the game closes.
                    if (game_ && game_->device == change.device)
                        game_.reset();
                }
                if (change.scalingChanged)
                {
                    // NVIDIA keeps its own; Windows' paths are written to its database now.
                    for (const auto& [device, before] : change.scalingBefore)
                    {
                        if (before.nvidia)
                            continue;
                        const PathScaling now = platform::ScalingOf(device);
                        if (Restorable(now) && now != before.path)
                            platform::SetScalingOf(device, now, true);
                    }
                }
            }

            void Revert() override
            {
                if (!pending_)
                    return;
                const Waiting change = std::move(*pending_);
                pending_.reset();
                if (change.customTrial)
                    nv_.RevertCustom(change.device);
                if (change.modeAfter && change.modeBefore)
                    ShowMode(change.device, *change.modeBefore);
                if (change.scalingChanged || change.modeAfter)
                    RestoreScaling(change.scalingBefore);
            }

            void Tick() override
            {
                if (pending_ && GetTickCount64() >= pending_->deadline)
                    Revert();
            }

            Outcome SwitchForGame(const std::string& device, const DisplayMode& wanted, std::optional<ScalingMode> scaling) override
            {
                Revert();  // an unanswered countdown doesn't survive a game starting
                const std::optional<DisplayMode> now = platform::CurrentModeOf(device);
                if (!now)
                    return { false, "Windows didn't report this display's mode." };
                DisplayMode mode = wanted;
                if (mode.refreshHz <= 0)
                    mode.refreshHz = now->refreshHz;  // keep the rate the player chose, where the size allows
                mode = Resolve(device, mode);

                GameSwitch change = game_ ? *game_ : GameSwitch{ device, *now, Snapshot() };
                if (!now->SameSize(mode) || now->refreshHz != mode.refreshHz)
                {
                    switch (platform::SwitchMode(device, mode))
                    {
                    case platform::ModeChange::Done: break;
                    case platform::ModeChange::BadMode: return { false, NotOffered(device, mode) };
                    default: return { false, "Windows refused " + ModeText(mode) + "." };
                    }
                }
                if (scaling)
                    PutScaling(device, *scaling);
                game_ = std::move(change);
                return { true, {} };
            }

            void EndGameSwitch() override
            {
                if (!game_)
                    return;
                const GameSwitch change = std::move(*game_);
                game_.reset();
                ShowMode(change.device, change.modeBefore);
                RestoreScaling(change.scalingBefore);
            }

            void ReturnToSaved(const std::string& device) override
            {
                const std::optional<DisplayMode> now = platform::CurrentModeOf(device);
                const std::optional<DisplayMode> saved = platform::SavedModeOf(device);
                if (now && saved && !(*now == *saved))
                    platform::SwitchBack(device);
            }

        private:
            struct Waiting
            {
                std::string device;
                std::optional<DisplayMode> modeBefore;
                ScalingSnapshot scalingBefore;
                std::optional<DisplayMode> modeAfter;
                bool scalingChanged = false;
                bool customTrial = false;  // an NVIDIA custom resolution on trial
                std::string description;
                ULONGLONG deadline = 0;
            };

            struct GameSwitch
            {
                std::string device;
                DisplayMode modeBefore;
                ScalingSnapshot scalingBefore;
            };

            enum class Put
            {
                Nothing,  // already so, or not something this display does
                Saved,    // set, but nothing on screen changes (the native mode)
                Shown,    // set, and the picture changes
                Refused,
            };

            ScalingSnapshot Snapshot()
            {
                ScalingSnapshot snapshot;
                for (const DisplayInfo& d : platform::AttachedDisplays())
                {
                    ScalingState state;
                    if (nv_.Drives(d.deviceName))
                    {
                        state.nvidia = true;
                        state.nv = nv_.Scaling(d.deviceName).value_or(-1);
                    }
                    else
                    {
                        state.path = platform::ScalingOf(d.deviceName);
                    }
                    snapshot[d.deviceName] = state;
                }
                return snapshot;
            }

            void RestoreScaling(const ScalingSnapshot& snapshot)
            {
                for (const auto& [device, before] : snapshot)
                {
                    if (before.nvidia)
                    {
                        if (before.nv >= 0 && nv_.Scaling(device) != before.nv)
                            nv_.SetScaling(device, before.nv);
                    }
                    else if (Restorable(before.path) && platform::ScalingOf(device) != before.path)
                    {
                        platform::SetScalingOf(device, before.path, false);
                    }
                }
            }

            static bool AtNative(const std::string& device)
            {
                const std::optional<DisplayMode> now = platform::CurrentModeOf(device);
                const std::optional<DisplayMode> native = platform::NativeModeOf(device);
                return now && native && now->SameSize(*native);
            }

            // Puts the user's scaling on a display. The NVIDIA driver keeps one setting per
            // display whatever the resolution; Windows' paths only scale below native.
            Put PutScaling(const std::string& device, ScalingMode mode)
            {
                if (nv_.Drives(device))
                {
                    const int wanted = ToNvidia(mode);
                    if (wanted < 0 || nv_.Scaling(device) == wanted)
                        return Put::Nothing;
                    if (!nv_.SetScaling(device, wanted))
                        return Put::Refused;
                    return AtNative(device) ? Put::Saved : Put::Shown;
                }
                if (AtNative(device))
                    return Put::Nothing;
                const PathScaling wanted = ToPath(mode);
                if (wanted == PathScaling::Unknown || platform::ScalingOf(device) == wanted)
                    return Put::Nothing;
                return platform::SetScalingOf(device, wanted, false) ? Put::Shown : Put::Refused;
            }

            // A mode the display doesn't list, made by the NVIDIA driver as a trial.
            // nullopt when there's no driver to ask (the caller explains instead).
            std::optional<Outcome> TryCustomMode(const std::string& device, const DisplayMode& mode, const DisplayMode& before, Waiting& change)
            {
                if (!nv_.Drives(device))
                    return std::nullopt;
                const std::optional<DisplayMode> native = platform::NativeModeOf(device);
                int fastest = 0;
                for (const DisplayMode& m : platform::ModesOf(device))
                    fastest = std::max(fastest, m.refreshHz);
                if (mode.width < 640 || mode.height < 480)
                    return Outcome{ false, SizeText(mode) + " is smaller than any display mode Windows supports." };
                if (native && (mode.width > native->width || mode.height > native->height))
                    return Outcome{ false, SizeText(mode) + " is bigger than this monitor's own " + SizeText(*native) + ". Nothing changed." };
                if (fastest > 0 && mode.refreshHz > fastest)
                    return Outcome{ false, "This monitor goes up to " + std::to_string(fastest) + " Hz. Nothing changed." };
                if (!nv_.TryCustom(device, mode.width, mode.height, mode.refreshHz))
                    return Outcome{ false, "Neither the display nor the NVIDIA driver would show " + ModeText(mode) + ". Nothing changed." };

                const std::optional<DisplayMode> showing = platform::CurrentModeOf(device);
                if (!showing || !showing->SameSize(mode))
                {
                    nv_.RevertCustom(device);
                    ShowMode(device, before);
                    return Outcome{ false, "The NVIDIA driver took " + ModeText(mode) + " but the display didn't switch. Nothing changed." };
                }
                change.customTrial = true;
                return Outcome{ true, {} };
            }

            static void ShowMode(const std::string& device, const DisplayMode& mode)
            {
                const std::optional<DisplayMode> now = platform::CurrentModeOf(device);
                if (now && *now == mode)
                    return;
                if (platform::SwitchMode(device, mode) != platform::ModeChange::Done)
                    platform::SwitchBack(device);  // the saved mode, which a trial never touches
            }

            // Refresh 0 means "the highest"; a rate the display doesn't offer at that size
            // becomes the nearest one below it (or the lowest above).
            static DisplayMode Resolve(const std::string& device, DisplayMode mode)
            {
                std::vector<int> rates;
                for (const DisplayMode& m : platform::ModesOf(device))
                {
                    if (m.SameSize(mode))
                        rates.push_back(m.refreshHz);
                }
                if (rates.empty() || std::find(rates.begin(), rates.end(), mode.refreshHz) != rates.end())
                    return mode;
                std::sort(rates.begin(), rates.end());
                if (mode.refreshHz <= 0)
                {
                    mode.refreshHz = rates.back();
                    return mode;
                }
                const auto above = std::upper_bound(rates.begin(), rates.end(), mode.refreshHz);
                mode.refreshHz = above == rates.begin() ? *above : *(above - 1);
                return mode;
            }

            static std::string NotOffered(const std::string& device, const DisplayMode& mode)
            {
                for (const DisplayMode& m : platform::ModesOf(device))
                {
                    if (m.SameSize(mode))
                        return "This display doesn't offer " + ModeText(mode) + ". Nothing changed.";
                }
                return "This display doesn't offer " + SizeText(mode) +
                       ". You can add it as a custom resolution in your graphics driver's control panel, then pick it here.";
            }

            platform::NvidiaDisplay nv_;
            std::optional<Waiting> pending_;
            std::optional<GameSwitch> game_;
            std::string integerKey_;
            bool integerOk_ = false;
        };
    }

    std::unique_ptr<IDisplayService> MakeDisplayService()
    {
        return std::make_unique<DisplayService>();
    }
}
