// Everything Radiance remembers between runs, and the store that writes it to
// %APPDATA%\Radiance\radiance.json (the earlier WinForms Radiance kept its own
// settings.json, profiles.json and games.json in the same folder; they are left alone).
#pragma once

#include "core/Model.h"

#include <climits>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace radiance
{
    struct AppSettings
    {
        // Window
        int windowX = INT_MIN;
        int windowY = INT_MIN;
        int lastPage = 0;
        int quickX = INT_MIN;  // where the quick popup was last left
        int quickY = INT_MIN;

        // Behaviour
        bool firstRunDone = false;
        bool startWithWindows = true;  // boots into the tray with Windows by default
        bool keepDesktopComposited = true;
        bool inGamePerformance = false;
        bool autoApplyProfiles = true;

        // Appearance
        std::string theme = "radiance";  // "radiance" (the UI source's look) or "macos"
        bool lightMode = false;           // each theme comes in dark and light
        bool particles = false;           // the drifting particle network behind the window

        // Colour
        ColourLook look;
        EyeCare eyeCare;
        // False until the user picks a vibrance: until then Radiance shows the NVIDIA
        // driver's own level and never writes it.
        bool vibranceChosen = false;
        std::string lookGame;  // which game's scene looks the picker shows
        std::string lookName;  // the look last applied, "" when the sliders were moved by hand
        std::vector<LookPreset> customLooks;

        // Shortcuts (Ctrl+Alt+V, Ctrl+Shift+M off, Ctrl+Alt+S)
        Hotkey quickPopup{ 3, 'V', true };
        Hotkey openMain{ 6, 'M', false };
        Hotkey screenshot{ 3, 'S', true };

        // Displays
        // How a lower resolution fills the screen. Unset until the user picks one; then
        // Radiance puts it on every resolution it switches to.
        std::optional<ScalingMode> scaling;
        bool monitorRulesEnabled = true;
        std::vector<MonitorRule> monitorRules;
        std::vector<PanelSettings> panels;
        std::vector<MonitorColour> monitorColours;

        // Games
        std::string selectedGameId;
        std::vector<GameProfile> profiles;
        std::string pendingGameOfferId;
        std::vector<std::string> declinedGameOffers;

        // While a game's profile is on screen: which game, and the look it replaced (put
        // back when the game closes, or on the next start if Radiance was interrupted).
        // `look` holds the profile's look meanwhile, so the Display page edits the profile.
        std::string activeProfile;
        std::optional<ColourLook> lookBeforeGame;
        // The display a game's launch resolution switched, until the game closes.
        std::string gameDisplayDevice;

        PanelSettings& PanelFor(int monitorIndex);
        MonitorColour& ColourFor(const std::string& deviceName);

        const GameProfile* ProfileFor(const std::string& gameId) const;
        void SetProfile(GameProfile profile);
        void RemoveProfile(const std::string& gameId);

        const MonitorRule* RuleFor(const std::string& gameId) const;
        void SetRule(const std::string& gameId, int width, int height);  // 0x0 clears it
    };

    class SettingsStore
    {
    public:
        explicit SettingsStore(std::filesystem::path file);

        static std::filesystem::path DefaultFolder();  // %APPDATA%\Radiance, or %RADIANCE_DATA_DIR%

        // False when there was no readable file, which is how a first run looks.
        bool Load(AppSettings& out) const;
        bool Save(const AppSettings& settings);

        // Coalesces bursts of changes (slider drags) into one write.
        void SaveSoon() { dirtySince_ = dirtySince_ ? dirtySince_ : Now(); }
        void FlushIfDue(const AppSettings& settings, bool force = false);

        const std::filesystem::path& File() const { return file_; }

    private:
        static uint64_t Now();

        std::filesystem::path file_;
        uint64_t dirtySince_ = 0;
    };
}
