#include "ui/pages/Shared.h"

#include "core/Catalog.h"
#include "services/Services.h"
#include "ui/Widgets.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace radiance::ui
{
    float Split(int count)
    {
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        return std::floor((ImGui::GetContentRegionAvail().x - gap * (count - 1)) / count) / Scale();
    }

    bool GameCombo(const char* id, std::string& gameId)
    {
        std::vector<std::string> names;
        int index = -1;
        int i = 0;
        for (const SupportedGame& g : catalog::Games())
        {
            names.emplace_back(g.displayName);
            if (gameId == g.id)
                index = i;
            ++i;
        }
        if (Combo(id, &index, names, "Pick a game") && index >= 0)
        {
            gameId = catalog::Games()[static_cast<size_t>(index)].id;
            return true;
        }
        return false;
    }

    std::string AspectLabel(int width, int height)
    {
        if (width <= 0 || height <= 0)
            return {};
        struct Known { double ratio; const char* label; };
        static constexpr Known kKnown[] = {
            { 16.0 / 9.0, "16:9" }, { 16.0 / 10.0, "16:10" }, { 4.0 / 3.0, "4:3" }, { 5.0 / 4.0, "5:4" },
            { 3.0 / 2.0, "3:2" }, { 21.0 / 9.0, "21:9" }, { 64.0 / 27.0, "21:9" }, { 43.0 / 18.0, "21:9" },
            { 32.0 / 9.0, "32:9" }, { 32.0 / 10.0, "32:10" },
        };
        const double r = static_cast<double>(width) / height;
        for (const Known& k : kKnown)
        {
            if (std::abs(r - k.ratio) < 0.02)
                return k.label;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f:1", r);
        return buf;
    }

    std::string ModeText(const DisplayMode& mode)
    {
        return std::to_string(mode.width) + " \xC3\x97 " + std::to_string(mode.height);
    }

    std::string Megapixels(int width, int height)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f MP", width * static_cast<double>(height) / 1e6);
        return buf;
    }

    std::string DescribeLook(const ColourLook& l)
    {
        std::string out;
        const auto add = [&](const std::string& part) {
            if (!out.empty())
                out += " \xC2\xB7 ";
            out += part;
        };
        const ColourLook n;
        if (l.saturation != n.saturation) add("Saturation " + Percent(l.saturation));
        if (l.vibrance != n.vibrance) add("Vibrance " + Percent(l.vibrance));
        if (l.brightness != n.brightness) add("Brightness " + Percent(l.brightness));
        if (l.gamma != n.gamma) add("Gamma " + Gamma(l.gamma));
        if (l.contrast != n.contrast) add("Contrast " + Percent(l.contrast));
        if (l.temperature != n.temperature) add("Temperature " + Signed(l.temperature));
        if (l.highlights || l.shadows || l.blacks || l.whites || l.shadowTint || l.highlightTint) add("Tone adjusted");
        if (l.hue || l.redGain != 100 || l.greenGain != 100 || l.blueGain != 100) add("Colour balance");
        return out.empty() ? "Changes nothing" : out;
    }

    bool SaturationAndVibrance(PageContext& ctx)
    {
        AppSettings& s = ctx.settings;
        IColourEngine& engine = *ctx.services.colour;

        static constexpr std::array<const char*, 4> kSatTicks{ "0", "100", "200", "300" };
        static constexpr std::array<const char*, 3> kDriverTicks{ "0", "100 \xC2\xB7 driver max", "350" };
        static constexpr std::array<float, 3> kDriverFractions{ 0.0f, 100.0f / 350.0f, 1.0f };
        static constexpr std::array<const char*, 3> kSoftwareTicks{ "0", "50 \xC2\xB7 neutral", "350" };
        static constexpr std::array<float, 3> kSoftwareFractions{ 0.0f, 50.0f / 350.0f, 1.0f };

        bool changed = false;
        const SliderSpec saturation{ "Saturation", 0, 300, 100, Percent, "How strong every colour is. Push it too far and detail turns to blocks." };
        changed |= HeroSlider("##saturation", &s.look.saturation, saturation, "100 is the game as it ships", kSatTicks);

        // Up to 100 vibrance is the NVIDIA driver's own digital vibrance (50 is its default);
        // past 100, or without the driver, Radiance applies it in software.
        const bool driver = engine.Driver() == DriverState::Available;
        std::string vibranceTip = "Boosts dull colours and mostly leaves skin tones alone. Subtler than saturation.";
        switch (engine.Driver())
        {
        case DriverState::Available: vibranceTip += "\n\nUp to 100 this is the NVIDIA driver's own digital vibrance, 50 being its default. Past 100 Radiance carries on in software."; break;
        case DriverState::DisplayNotOnNvidia: vibranceTip += "\n\nApplied by Radiance rather than the NVIDIA driver - no display is on the NVIDIA GPU. 50 leaves colours alone."; break;
        default: vibranceTip += "\n\nApplied by Radiance rather than a driver - no NVIDIA driver here. 50 leaves colours alone."; break;
        }
        SliderSpec vibrance{ "Vibrance", 0, 350, driver ? std::optional<int>(100) : std::nullopt, Percent, vibranceTip.c_str() };
        vibrance.origin = 0;
        vibrance.neutral = engine.DriverDefault();
        const char* vibranceHint = !driver ? "Applied in software" : (s.vibranceChosen ? "Past 100 it continues in software" : "The driver's own level, untouched");
        changed |= HeroSlider("##vibrance", &s.look.vibrance, vibrance, vibranceHint, driver ? kDriverTicks : kSoftwareTicks, driver ? kDriverFractions : kSoftwareFractions);
        return changed;
    }
}
