// UTF-8 <-> UTF-16 at the Windows API boundary. Radiance keeps text as UTF-8
// everywhere else (ImGui draws UTF-8).
#pragma once

#include <string>
#include <string_view>

namespace radiance::platform
{
    std::string Narrow(std::wstring_view wide);
    std::wstring Widen(std::string_view utf8);
}
