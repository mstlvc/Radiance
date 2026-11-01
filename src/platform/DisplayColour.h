// The two Windows-side colour stages: each display's gamma ramp (GDI), and the
// full-screen colour filter (Magnification API).
#pragma once

#include "core/ColourMath.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace radiance::platform
{
    // Writes gamma ramps per display, remembers what each display had before Radiance
    // touched it, and eases a ramp toward neutral when Windows refuses the full one.
    class GammaRamps
    {
    public:
        struct Result
        {
            bool ok = true;       // every display took a ramp (maybe a softened one)
            int refused = 0;      // displays that refused even the gentlest ramp
            int softened = 0;     // displays that only took a gentler ramp
        };

        // Attached displays (\\.\DISPLAY1...).
        static std::vector<std::string> Displays();

        // `ramp` nullopt means "neutral": hand the display back what it had.
        bool Apply(const std::string& device, const std::optional<colour::Ramp>& ramp, Result& result);
        // True when the display still shows what Radiance last wrote.
        bool StillHolds(const std::string& device) const;
        // Forgets what was written so the next Apply writes again (after a reset).
        void Forget(const std::string& device);
        // Puts every display Radiance coloured back the way it found it.
        void RestoreAll();

        static std::optional<colour::Ramp> Read(const std::string& device);
        static bool Write(const std::string& device, const colour::Ramp& ramp);

    private:
        struct DisplayState
        {
            std::optional<colour::Ramp> original;  // their own ramp (calibration), when it looked like one
            std::optional<colour::Ramp> written;   // what Radiance last asked for there
            std::optional<colour::Ramp> softened;  // the gentler ramp Windows took instead, if any
            bool coloured = false;
        };
        std::map<std::string, DisplayState> displays_;
    };

    // The Magnification API's full-screen colour effect. Must be used from one thread
    // (Radiance's UI thread); the effect goes away with the process.
    class ScreenColourFilter
    {
    public:
        ScreenColourFilter() = default;
        ~ScreenColourFilter();
        ScreenColourFilter(const ScreenColourFilter&) = delete;
        ScreenColourFilter& operator=(const ScreenColourFilter&) = delete;

        bool Available();
        // nullopt clears the effect.
        bool Set(const std::optional<colour::Matrix>& matrix);
        bool Active() const { return active_; }

    private:
        bool tried_ = false;
        bool initialised_ = false;
        bool active_ = false;
        void* module_ = nullptr;
        void* setEffect_ = nullptr;
        void* uninitialize_ = nullptr;
    };
}
