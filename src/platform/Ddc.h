// DDC/CI through Windows' monitor configuration API (dxva2): the physical monitors
// behind the desktop, VCP reads and writes, and the capabilities string. Every call
// here talks to the monitor over the cable and can take tens to hundreds of
// milliseconds, so none of it belongs on the UI thread.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace radiance::platform
{
    // Opens every physical monitor on the desktop; closes them again on destruction.
    // Opening doesn't talk to the monitors, so it's cheap.
    class PhysicalMonitors
    {
    public:
        struct Monitor
        {
            void* handle = nullptr;
            std::string description;  // what the driver calls it, often "Generic PnP Monitor"
            std::string device;       // the Windows display it shows (\\.\DISPLAY1)
        };

        PhysicalMonitors();
        ~PhysicalMonitors();
        PhysicalMonitors(const PhysicalMonitors&) = delete;
        PhysicalMonitors& operator=(const PhysicalMonitors&) = delete;

        const std::vector<Monitor>& List() const { return monitors_; }

    private:
        std::vector<Monitor> monitors_;
    };

    struct VcpReading
    {
        int current = 0;
        int maximum = 0;
    };

    std::optional<VcpReading> ReadVcp(void* monitor, uint8_t code);
    bool WriteVcp(void* monitor, uint8_t code, int value);
    // The monitor's MCCS capabilities string; "" when it doesn't give one.
    std::string CapabilitiesOf(void* monitor);
}
