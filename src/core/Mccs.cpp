#include "core/Mccs.h"

#include <cctype>
#include <cstdio>

namespace radiance::mccs
{
    namespace
    {
        int HexValue(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        // Finds "vcp(" as a keyword of its own (not the tail of "vcpname(" or "mccs_vcp(").
        size_t FindVcpSection(std::string_view s)
        {
            for (size_t i = 0; i + 4 <= s.size(); ++i)
            {
                const bool word = std::tolower(static_cast<unsigned char>(s[i])) == 'v' &&
                                  std::tolower(static_cast<unsigned char>(s[i + 1])) == 'c' &&
                                  std::tolower(static_cast<unsigned char>(s[i + 2])) == 'p' && s[i + 3] == '(';
                const bool starts = i == 0 || (!std::isalnum(static_cast<unsigned char>(s[i - 1])) && s[i - 1] != '_');
                if (word && starts)
                    return i + 4;
            }
            return std::string_view::npos;
        }
    }

    std::map<uint8_t, std::vector<uint8_t>> ParseVcp(std::string_view s)
    {
        std::map<uint8_t, std::vector<uint8_t>> codes;
        size_t i = FindVcpSection(s);
        if (i == std::string_view::npos)
            return codes;

        // Tokens are hex bytes; a "(" after a code opens that code's value list.
        int depth = 0;
        int lastCode = -1;
        while (i < s.size())
        {
            const char c = s[i];
            if (c == '(')
            {
                ++depth;
                ++i;
                continue;
            }
            if (c == ')')
            {
                if (depth == 0)
                    break;  // the end of vcp(...)
                --depth;
                ++i;
                continue;
            }
            const int hi = HexValue(c);
            const int lo = i + 1 < s.size() ? HexValue(s[i + 1]) : -1;
            if (hi >= 0 && lo >= 0)
            {
                const auto value = static_cast<uint8_t>(hi * 16 + lo);
                if (depth == 0)
                {
                    lastCode = value;
                    codes[value];
                }
                else if (depth == 1 && lastCode >= 0)
                {
                    codes[static_cast<uint8_t>(lastCode)].push_back(value);
                }
                i += 2;
                continue;
            }
            ++i;  // spaces and anything unexpected
        }
        return codes;
    }

    std::string InputSourceName(uint8_t value)
    {
        switch (value)
        {
        case 0x01: return "VGA 1";
        case 0x02: return "VGA 2";
        case 0x03: return "DVI 1";
        case 0x04: return "DVI 2";
        case 0x05: return "Composite 1";
        case 0x06: return "Composite 2";
        case 0x07: return "S-Video 1";
        case 0x08: return "S-Video 2";
        case 0x09: return "Tuner 1";
        case 0x0A: return "Tuner 2";
        case 0x0B: return "Tuner 3";
        case 0x0C: return "Component 1";
        case 0x0D: return "Component 2";
        case 0x0E: return "Component 3";
        case 0x0F: return "DisplayPort 1";
        case 0x10: return "DisplayPort 2";
        case 0x11: return "HDMI 1";
        case 0x12: return "HDMI 2";
        default: break;
        }
        char buf[24];
        std::snprintf(buf, sizeof(buf), "Input 0x%02X", value);
        return buf;
    }
}
