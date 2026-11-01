// GPU scaling and custom resolutions through NVAPI: the settings behind NVIDIA
// Control Panel's "Adjust desktop size and position" and "Customize..." resolutions.
// Loaded at runtime from nvapi64.dll, like the vibrance code; everything reports
// "unavailable" on PCs without the driver.
#pragma once

#include <optional>
#include <string>

namespace radiance::platform
{
    // NVIDIA's NV_SCALING values Radiance uses.
    namespace NvScaling
    {
        constexpr int ToNative = 2;   // full screen (stretched)
        constexpr int Centered = 3;   // no scaling
        constexpr int Aspect = 5;     // aspect ratio
        constexpr int Integer = 8;    // whole-number multiples, Turing and later
    }

    class NvidiaDisplay
    {
    public:
        NvidiaDisplay() = default;
        ~NvidiaDisplay();
        NvidiaDisplay(const NvidiaDisplay&) = delete;
        NvidiaDisplay& operator=(const NvidiaDisplay&) = delete;

        bool Available();
        // True when this Windows display (\\.\DISPLAY1) is on the NVIDIA GPU.
        bool Drives(const std::string& device);

        // The driver's scaling for the display, as an NV_SCALING value.
        std::optional<int> Scaling(const std::string& device);
        // Sets it, kept by the driver across restarts. `validateOnly` asks the driver
        // whether it would take it and changes nothing.
        bool SetScaling(const std::string& device, int nvScaling, bool validateOnly = false);

        // Custom resolutions. Timing() only computes (nothing changes); TryCustom() shows
        // the mode now as a trial, until SaveCustom() keeps it or RevertCustom() drops it.
        bool Timing(const std::string& device, int width, int height, int refreshHz);
        bool TryCustom(const std::string& device, int width, int height, int refreshHz);
        bool SaveCustom(const std::string& device);
        bool RevertCustom(const std::string& device);
        // How many custom resolutions the driver holds for the display.
        int CustomCount(const std::string& device);

        // The driver's answer to the last set or trial (NvAPI_Status, 0 is success).
        int LastStatus() const { return lastStatus_; }

    private:
        struct Api;
        std::optional<unsigned> DisplayId(const std::string& device);

        bool tried_ = false;
        int lastStatus_ = 0;
        void* module_ = nullptr;
        Api* api_ = nullptr;
    };
}
