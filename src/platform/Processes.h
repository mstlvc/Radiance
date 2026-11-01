// Which programs are running, for game detection. One Toolhelp snapshot: a couple
// of milliseconds, no handles kept.
#pragma once

#include <string>
#include <vector>

namespace radiance::platform
{
    // Executable names of every running process, lower case, without ".exe".
    std::vector<std::string> RunningProcessNames();
}
