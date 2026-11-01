// The DDC/CI (MCCS) vocabulary Radiance uses: the VCP codes, reading a monitor's
// capabilities string, and the standard names for input sources. Pure functions.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace radiance::mccs
{
    constexpr uint8_t kBrightness = 0x10;
    constexpr uint8_t kContrast = 0x12;
    constexpr uint8_t kRedGain = 0x16;
    constexpr uint8_t kGreenGain = 0x18;
    constexpr uint8_t kBlueGain = 0x1A;
    constexpr uint8_t kInputSource = 0x60;

    // The vcp(...) section of a capabilities string: each code the monitor lists, with
    // the values it lists for it ("60(0F 11 12)" -> {0x60: {0x0F, 0x11, 0x12}}).
    // Empty when the string has no vcp section.
    std::map<uint8_t, std::vector<uint8_t>> ParseVcp(std::string_view capabilities);

    // "HDMI 1", "DisplayPort 2"... as the standard numbers them; "Input 0x1B" otherwise.
    std::string InputSourceName(uint8_t value);
}
