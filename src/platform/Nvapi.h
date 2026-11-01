// NVIDIA Digital Vibrance through NVAPI, loaded at runtime from nvapi64.dll (the
// driver installs it), so Radiance runs fine on PCs without an NVIDIA card.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace radiance::platform
{
    struct VibranceLevel
    {
        int current = 0;
        int minimum = 0;
        int maximum = 100;
        int standard = 50;  // the driver's default
    };

    class NvidiaVibrance
    {
    public:
        NvidiaVibrance() = default;
        ~NvidiaVibrance();
        NvidiaVibrance(const NvidiaVibrance&) = delete;
        NvidiaVibrance& operator=(const NvidiaVibrance&) = delete;

        enum class State { NotLoaded, NoDriver, NoDisplays, Ready };

        // Loads the driver's API and finds the displays it drives. Safe to call again
        // (displays come and go).
        State Initialise();
        State GetState() const { return state_; }

        size_t DisplayCount() const { return displays_.size(); }
        // The Windows device (\\.\DISPLAY1) an NVIDIA display drives, "" when unknown.
        std::string DeviceName(size_t display) const;
        std::optional<VibranceLevel> Read(size_t display) const;
        bool Write(size_t display, int level);

    private:
        struct Functions;

        State state_ = State::NotLoaded;
        void* module_ = nullptr;
        Functions* fn_ = nullptr;
        std::vector<void*> displays_;
    };
}
