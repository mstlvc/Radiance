#include "platform/Ddc.h"

#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

#include <cstring>

namespace radiance::platform
{
    namespace
    {
        BOOL CALLBACK Collect(HMONITOR monitor, HDC, LPRECT, LPARAM data)
        {
            reinterpret_cast<std::vector<HMONITOR>*>(data)->push_back(monitor);
            return TRUE;
        }
    }

    PhysicalMonitors::PhysicalMonitors()
    {
        std::vector<HMONITOR> desktops;
        EnumDisplayMonitors(nullptr, nullptr, &Collect, reinterpret_cast<LPARAM>(&desktops));
        for (HMONITOR desktop : desktops)
        {
            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            const bool named = GetMonitorInfoW(desktop, &info) != FALSE;

            DWORD count = 0;
            if (!GetNumberOfPhysicalMonitorsFromHMONITOR(desktop, &count) || count == 0)
                continue;
            std::vector<PHYSICAL_MONITOR> physical(count);
            if (!GetPhysicalMonitorsFromHMONITOR(desktop, count, physical.data()))
                continue;
            for (const PHYSICAL_MONITOR& p : physical)
                monitors_.push_back(Monitor{ p.hPhysicalMonitor, Narrow(p.szPhysicalMonitorDescription), named ? Narrow(info.szDevice) : std::string() });
        }
    }

    PhysicalMonitors::~PhysicalMonitors()
    {
        for (const Monitor& m : monitors_)
            DestroyPhysicalMonitor(m.handle);
    }

    std::optional<VcpReading> ReadVcp(void* monitor, uint8_t code)
    {
        DWORD current = 0, maximum = 0;
        MC_VCP_CODE_TYPE type{};
        if (!GetVCPFeatureAndVCPFeatureReply(monitor, code, &type, &current, &maximum))
            return std::nullopt;
        return VcpReading{ static_cast<int>(current), static_cast<int>(maximum) };
    }

    bool WriteVcp(void* monitor, uint8_t code, int value)
    {
        return SetVCPFeature(monitor, code, static_cast<DWORD>(value)) != FALSE;
    }

    std::string CapabilitiesOf(void* monitor)
    {
        DWORD length = 0;
        if (!GetCapabilitiesStringLength(monitor, &length) || length == 0)
            return {};
        std::string text(length, '\0');
        if (!CapabilitiesRequestAndCapabilitiesReply(monitor, text.data(), length))
            return {};
        text.resize(strnlen(text.c_str(), length));
        return text;
    }
}
