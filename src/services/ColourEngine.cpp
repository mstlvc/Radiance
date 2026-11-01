// The real colour engine. A look reaches the screen in three stages:
//  1. NVIDIA Digital Vibrance, the driver's 0..100 level, on displays an NVIDIA GPU drives;
//  2. each display's gamma ramp: gamma, the tone curve, brightness, contrast, temperature,
//     RGB gains, eye care and the display's own gamma / warmth;
//  3. the full-screen colour filter: saturation, vibrance past the driver's range (or all
//     of it without the driver), hue and colour-vision correction.
// HandBack() undoes all three: the driver goes back to the level Radiance found, each
// display to the ramp it had, and the filter is cleared.
#include "services/Services.h"

#include "core/ColourMath.h"
#include "platform/DisplayColour.h"
#include "platform/Nvapi.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <map>

namespace radiance
{
    namespace
    {
        using platform::GammaRamps;
        using platform::NvidiaVibrance;

        std::string Plural(int n, const char* one, const char* many)
        {
            return std::to_string(n) + " " + (n == 1 ? one : many);
        }

        class ColourEngine final : public IColourEngine
        {
        public:
            ColourEngine()
            {
                nv_.Initialise();
                displays_ = GammaRamps::Displays();
                if (nv_.GetState() == NvidiaVibrance::State::Ready)
                {
                    if (const auto level = nv_.Read(PrimaryNvidiaDisplay()))
                    {
                        levelAtStart_ = level->current;
                        driverDefault_ = level->standard;
                    }
                }
            }

            ~ColourEngine() override { HandBack(); }

            void Apply(const ColourTarget& target) override
            {
                target_ = target;
                applied_ = true;
                Push();
            }

            void SetCompare(bool showNeutral) override
            {
                if (compare_ == showNeutral)
                    return;
                compare_ = showNeutral;
                if (applied_)
                    Push();
            }

            bool Comparing() const override { return compare_; }

            void Reassert() override
            {
                if (!applied_)
                    return;
                bool needed = RefreshDisplays();
                if (WantsDriver())
                {
                    const int level = colour::DriverLevel(target_.look.vibrance);
                    for (size_t i = 0; i < nv_.DisplayCount(); ++i)
                    {
                        const auto now = nv_.Read(i);
                        if (now && now->current != level)
                            needed = true;
                    }
                }
                for (const std::string& device : displays_)
                {
                    if (!ramps_.StillHolds(device))
                    {
                        ramps_.Forget(device);  // a game or a mode change reset it: write again
                        needed = true;
                    }
                }
                if (needed)
                    Push();
            }

            void HandBack() override
            {
                filter_.Set(std::nullopt);
                ramps_.RestoreAll();
                RestoreDriver();
                levelFound_.clear();
                applied_ = false;
            }

            ColourHealth Health() const override { return health_; }

            DriverState Driver() const override
            {
                switch (nv_.GetState())
                {
                case NvidiaVibrance::State::Ready: return DriverState::Available;
                case NvidiaVibrance::State::NoDisplays: return DriverState::DisplayNotOnNvidia;
                case NvidiaVibrance::State::NoDriver: return DriverState::NoNvidiaCard;
                default: return DriverState::Unknown;
                }
            }

            std::optional<int> DriverLevelAtStart() const override { return levelAtStart_; }
            int DriverDefault() const override { return driverDefault_; }
            std::string StatusLine() const override { return status_; }

        private:
            bool DriverReady() const { return nv_.GetState() == NvidiaVibrance::State::Ready; }
            bool WantsDriver() const { return DriverReady() && target_.driverVibranceChosen && !compare_; }

            // The NVIDIA display showing the Windows primary, or the first one.
            size_t PrimaryNvidiaDisplay() const
            {
                DISPLAY_DEVICEA dd{};
                dd.cb = sizeof(dd);
                for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i)
                {
                    if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
                    {
                        for (size_t n = 0; n < nv_.DisplayCount(); ++n)
                        {
                            if (_stricmp(nv_.DeviceName(n).c_str(), dd.DeviceName) == 0)
                                return n;
                        }
                        break;
                    }
                    dd = DISPLAY_DEVICEA{};
                    dd.cb = sizeof(dd);
                }
                return 0;
            }

            std::string DriverKey(size_t display) const
            {
                std::string name = nv_.DeviceName(display);
                return name.empty() ? "#" + std::to_string(display) : name;
            }

            // Displays come and go (and NVAPI's handles with them). True when they changed.
            bool RefreshDisplays()
            {
                std::vector<std::string> now = GammaRamps::Displays();
                if (now == displays_)
                    return false;
                displays_ = std::move(now);
                if (nv_.GetState() != NvidiaVibrance::State::NoDriver)
                    nv_.Initialise();
                return true;
            }

            void RestoreDriver()
            {
                if (!DriverReady())
                    return;
                for (size_t i = 0; i < nv_.DisplayCount(); ++i)
                {
                    const auto found = levelFound_.find(DriverKey(i));
                    if (found == levelFound_.end())
                        continue;
                    const auto now = nv_.Read(i);
                    if (now && now->current != found->second)
                        nv_.Write(i, found->second);
                }
            }

            void Push()
            {
                RefreshDisplays();
                const ColourLook& look = target_.look;

                // 1. NVIDIA vibrance.
                int driverDisplays = 0;
                int driverFailed = 0;
                if (WantsDriver())
                {
                    const int level = colour::DriverLevel(look.vibrance);
                    for (size_t i = 0; i < nv_.DisplayCount(); ++i)
                    {
                        const auto now = nv_.Read(i);
                        if (!now)
                        {
                            ++driverFailed;
                            continue;
                        }
                        levelFound_.try_emplace(DriverKey(i), now->current);  // first touch: remember it
                        if (now->current != level && !nv_.Write(i, level))
                            ++driverFailed;
                        else
                            ++driverDisplays;
                    }
                }
                else
                {
                    RestoreDriver();  // comparing, or the level was never chosen
                }

                // 2. Gamma ramps, one per display.
                GammaRamps::Result ramps;
                int coloured = 0;
                for (const std::string& device : displays_)
                {
                    std::optional<colour::Ramp> ramp;
                    if (!compare_)
                    {
                        int gamma = 100, warmth = 0;
                        for (const MonitorColour& m : target_.monitors)
                        {
                            if (m.deviceName == device)
                            {
                                gamma = m.gammaPercent;
                                warmth = m.warmthPercent;
                            }
                        }
                        const colour::Ramp wanted = colour::DisplayRamp(look, target_.eyeCare, gamma, warmth);
                        if (!colour::IsIdentity(wanted))
                            ramp = wanted;
                    }
                    if (ramps_.Apply(device, ramp, ramps) && ramp)
                        ++coloured;
                }

                // 3. The colour filter.
                const std::optional<colour::Matrix> matrix = compare_ ? std::nullopt : colour::ColourMatrix(look, DriverReady());
                const bool filterOk = filter_.Set(matrix) || !matrix;

                Describe(driverDisplays, driverFailed, coloured, ramps, matrix.has_value(), filterOk);
            }

            void Describe(int driverDisplays, int driverFailed, int coloured, const GammaRamps::Result& ramps, bool wantsFilter, bool filterOk)
            {
                std::vector<std::string> parts;
                int problems = 0;
                int working = 0;

                if (compare_)
                {
                    parts.push_back("Comparing: showing the screen as Radiance found it");
                }
                else
                {
                    switch (nv_.GetState())
                    {
                    case NvidiaVibrance::State::Ready:
                        if (!target_.driverVibranceChosen)
                            parts.push_back("NVIDIA vibrance left as the driver has it");
                        else if (driverFailed)
                            parts.push_back("NVIDIA vibrance refused on " + Plural(driverFailed, "display", "displays"));
                        else
                            parts.push_back("NVIDIA vibrance " + std::to_string(colour::DriverLevel(target_.look.vibrance)) + " on " + Plural(driverDisplays, "display", "displays"));
                        break;
                    case NvidiaVibrance::State::NoDisplays:
                        parts.push_back("No display on the NVIDIA GPU: vibrance in software");
                        break;
                    default:
                        parts.push_back("No NVIDIA driver: vibrance in software");
                        break;
                    }
                    problems += driverFailed;
                    working += driverDisplays;

                    if (ramps.refused)
                        parts.push_back("Windows refused the gamma ramp on " + Plural(ramps.refused, "display", "displays"));
                    if (ramps.softened)
                        parts.push_back("gamma softened on " + Plural(ramps.softened, "display", "displays") + " (Windows limits the range)");
                    if (coloured && !ramps.refused && !ramps.softened)
                        parts.push_back("gamma ramp on " + Plural(coloured, "display", "displays"));
                    problems += ramps.refused + ramps.softened;
                    working += coloured;

                    if (wantsFilter)
                    {
                        parts.push_back(filterOk ? "colour filter on" : "colour filter unavailable");
                        problems += filterOk ? 0 : 1;
                        working += filterOk ? 1 : 0;
                    }
                }

                status_.clear();
                for (size_t i = 0; i < parts.size(); ++i)
                    status_ += (i ? " \xC2\xB7 " : "") + parts[i];

                if (problems == 0)
                    health_ = ColourHealth::Live;
                else
                    health_ = working > 0 ? ColourHealth::Limited : ColourHealth::Off;
            }

            NvidiaVibrance nv_;
            GammaRamps ramps_;
            platform::ScreenColourFilter filter_;

            std::vector<std::string> displays_;
            std::map<std::string, int> levelFound_;  // driver level per display before Radiance wrote it
            std::optional<int> levelAtStart_;
            int driverDefault_ = 50;

            ColourTarget target_;
            bool applied_ = false;
            bool compare_ = false;
            ColourHealth health_ = ColourHealth::Live;
            std::string status_ = "Starting";
        };
    }

    std::unique_ptr<IColourEngine> MakeColourEngine()
    {
        return std::make_unique<ColourEngine>();
    }
}
