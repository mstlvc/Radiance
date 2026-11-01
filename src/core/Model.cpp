#include "core/Model.h"

#include <algorithm>
#include <cmath>

namespace radiance
{
    namespace
    {
        std::string KeyName(uint32_t vk)
        {
            if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
                return std::string(1, static_cast<char>(vk));
            if (vk >= 0x70 && vk <= 0x87)  // F1..F24
                return "F" + std::to_string(vk - 0x70 + 1);
            if (vk >= 0x60 && vk <= 0x69)  // numpad digits
                return "Num " + std::to_string(vk - 0x60);
            switch (vk)
            {
            case 0x20: return "Space";
            case 0x09: return "Tab";
            case 0x0D: return "Enter";
            case 0x08: return "Backspace";
            case 0x2E: return "Delete";
            case 0x2D: return "Insert";
            case 0x24: return "Home";
            case 0x23: return "End";
            case 0x21: return "Page Up";
            case 0x22: return "Page Down";
            case 0x25: return "Left";
            case 0x26: return "Up";
            case 0x27: return "Right";
            case 0x28: return "Down";
            case 0x13: return "Pause";
            case 0x2C: return "Print Screen";
            case 0x91: return "Scroll Lock";
            case 0xC0: return "`";
            case 0xBD: return "-";
            case 0xBB: return "=";
            case 0xDB: return "[";
            case 0xDD: return "]";
            case 0xDC: return "\\";
            case 0xBA: return ";";
            case 0xDE: return "'";
            case 0xBC: return ",";
            case 0xBE: return ".";
            case 0xBF: return "/";
            case 0x6A: return "Num *";
            case 0x6B: return "Num +";
            case 0x6D: return "Num -";
            case 0x6E: return "Num .";
            case 0x6F: return "Num /";
            default: return "Key " + std::to_string(vk);
            }
        }
    }

    std::string Hotkey::Describe() const
    {
        if (!IsSet())
            return "Not set";
        std::string text;
        const auto add = [&](const char* part) {
            if (!text.empty())
                text += " + ";
            text += part;
        };
        if (modifiers & 2) add("Ctrl");
        if (modifiers & 1) add("Alt");
        if (modifiers & 4) add("Shift");
        if (modifiers & 8) add("Win");
        add(KeyName(virtualKey).c_str());
        return text;
    }

    int VcpRange::ToPercent(int raw) const
    {
        const int span = std::max(1, maximum - minimum);
        return std::clamp(static_cast<int>(std::lround(100.0 * (raw - minimum) / span)), 0, 100);
    }

    int VcpRange::FromPercent(int percent) const
    {
        const int span = std::max(1, maximum - minimum);
        return minimum + static_cast<int>(std::lround(span * std::clamp(percent, 0, 100) / 100.0));
    }
}
