#include "core/Catalog.h"

#include <array>

namespace radiance::catalog
{
    namespace
    {
        constexpr std::array<SupportedGame, 6> kGames{ {
            { "rust", "Rust", "RustClient", "Rust" },
            { "cs2", "Counter-Strike 2", "cs2", "Counter-Strike 2" },
            { "apex", "Apex Legends", "r5apex", nullptr },
            { "fortnite", "Fortnite", "FortniteClient-Win64-Shipping", "Fortnite" },
            { "valorant", "VALORANT", "VALORANT-Win64-Shipping", nullptr },
            { "pubg", "PUBG: BATTLEGROUNDS", "TslGame", "PUBG" },
        } };

        struct LookRow
        {
            const char* game;
            const char* name;
            const char* art;
            ColourLook look;
        };

        // Starting values for the shipped scene looks. Radiance has no backend, so
        // they live here. Tune freely. Vibrance is the NVIDIA scale: 50 is the driver's
        // default, 100 its maximum.
        ColourLook L(int sat, int vib, int bright = 100, int gamma = 100, int contrast = 100, int temp = 0, int highlights = 0, int shadows = 0, int blacks = 0)
        {
            ColourLook l;
            l.saturation = sat;
            l.vibrance = vib;
            l.brightness = bright;
            l.gamma = gamma;
            l.contrast = contrast;
            l.temperature = temp;
            l.highlights = highlights;
            l.shadows = shadows;
            l.blacks = blacks;
            return l;
        }

        const std::vector<LookRow>& Rows()
        {
            static const std::vector<LookRow> rows{
                { "Rust", "Balanced", "rust-balanced", L(130, 70, 100, 100, 105, 0, 0, 10) },
                { "Rust", "Forest", "rust-forest", L(140, 80, 100, 95, 108, -5, -5, 15) },
                { "Rust", "Desert", "rust-desert", L(125, 75, 100, 100, 110, -10, -10, 0) },
                { "Rust", "Snow", "rust-snow", L(120, 65, 95, 100, 112, 5, -20, 0) },
                { "Rust", "Black Holo", "rust-blackholo", L(150, 90, 100, 115, 104, 0, 0, 30, 10) },
                { "Rust", "Vivid", "rust-vivid", L(170, 110, 100, 100, 108) },

                { "Counter-Strike 2", "Balanced", "cs2-balanced", L(130, 75) },
                { "Counter-Strike 2", "Dust", "cs2-dust", L(120, 70, 100, 100, 104, -8, -8, 0) },
                { "Counter-Strike 2", "Concrete", "cs2-concrete", L(140, 85, 100, 100, 110) },
                { "Counter-Strike 2", "Shadows", "cs2-shadows", L(125, 70, 100, 112, 100, 0, 0, 25) },
                { "Counter-Strike 2", "Vivid", "cs2-vivid", L(170, 115, 100, 100, 106) },

                { "Fortnite", "Balanced", "fortnite-balanced", L(120, 70) },
                { "Fortnite", "Daylight", "fortnite-daylight", L(115, 65, 100, 100, 104, 0, -10, 0) },
                { "Fortnite", "Town", "fortnite-town", L(135, 80, 100, 100, 106) },
                { "Fortnite", "Night", "fortnite-night", L(125, 75, 100, 115, 100, 0, 0, 25) },
                { "Fortnite", "Vivid", "fortnite-vivid", L(160, 105, 100, 100, 106) },

                { "PUBG", "Balanced", "pubg-balanced", L(130, 75) },
                { "PUBG", "Desert", "pubg-desert", L(125, 75, 100, 100, 106, -10, -10, 0) },
                { "PUBG", "Jungle", "pubg-jungle", L(140, 82, 100, 100, 104, 0, 0, 15) },
                { "PUBG", "Urban", "pubg-urban", L(135, 80, 100, 100, 108) },
                { "PUBG", "Vivid", "pubg-vivid", L(170, 110, 100, 100, 108) },
            };
            return rows;
        }

        constexpr std::array<PvpResolution, 3> kPvp{ {
            { "Native", 1920, 1080, "16:9", "Sharpest picture and the full field of view.", "Nothing is stretched or cropped.", false },
            { "Stretched 4:3", 1440, 1080, "4:3", "Player models come out wider and it runs faster than native.", "You lose some view at the left and right.", true },
            { "Low 4:3", 1280, 960, "4:3", "The most frames per second.", "The softest picture of the three.", true },
        } };

        constexpr std::array<VisionMode, 4> kVision{ {
            { ColourVision::Normal, "Normal", "No correction." },
            { ColourVision::GreenWeak, "Green-weak", "Deuteranopia - by far the most common, around 6% of men." },
            { ColourVision::RedWeak, "Red-weak", "Protanopia - reds look darker and closer to green." },
            { ColourVision::BlueWeak, "Blue-weak", "Tritanopia - rare, and affects blue against green." },
        } };

        constexpr std::array<ScalingChoice, 4> kScaling{ {
            { ScalingMode::AspectRatio, "Aspect ratio" },
            { ScalingMode::NoScaling, "No scaling" },
            { ScalingMode::FullScreen, "Full screen" },
            { ScalingMode::Integer, "Integer" },
        } };
    }

    std::span<const SupportedGame> Games()
    {
        return kGames;
    }

    const SupportedGame* GameById(std::string_view id)
    {
        for (const SupportedGame& g : kGames)
        {
            if (id == g.id)
                return &g;
        }
        return nullptr;
    }

    std::vector<std::string> LookGames()
    {
        std::vector<std::string> games;
        for (const LookRow& row : Rows())
        {
            if (games.empty() || games.back() != row.game)
                games.emplace_back(row.game);
        }
        return games;
    }

    std::vector<LookPreset> LooksFor(std::string_view game)
    {
        std::vector<LookPreset> looks;
        for (const LookRow& row : Rows())
        {
            if (game == row.game)
                looks.push_back(LookPreset{ row.game, row.name, row.art, row.look });
        }
        return looks;
    }

    std::span<const PvpResolution> PvpResolutions()
    {
        return kPvp;
    }

    std::span<const VisionMode> VisionModes()
    {
        return kVision;
    }

    std::span<const Utility> Utilities()
    {
        static constexpr std::array<Utility, 1> kUtilities{ {
            { "winutil", "Windows Utility", "Chris Titus Tech",
                "Install apps, tweak and debloat Windows, repair updates.",
                "Installs common programs in one go, applies performance and privacy tweaks, removes preinstalled bloat and "
                "repairs Windows Update. It opens its own window, and nothing changes until you pick something there.",
                "irm https://christitus.com/win | iex", "https://github.com/ChrisTitusTech/winutil", true },
        } };
        return kUtilities;
    }

    std::span<const ScalingChoice> ScalingChoices()
    {
        return kScaling;
    }
}
