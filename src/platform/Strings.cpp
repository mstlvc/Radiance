#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace radiance::platform
{
    std::string Narrow(std::wstring_view wide)
    {
        if (wide.empty())
            return {};
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), bytes, nullptr, nullptr);
        return out;
    }

    std::wstring Widen(std::string_view utf8)
    {
        if (utf8.empty())
            return {};
        const int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(chars), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), chars);
        return out;
    }
}
