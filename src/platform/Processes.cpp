#include "platform/Processes.h"

#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>

namespace radiance::platform
{
    std::vector<std::string> RunningProcessNames()
    {
        std::vector<std::string> names;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return names;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry))
        {
            std::string name = Narrow(entry.szExeFile);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name.size() > 4 && name.ends_with(".exe"))
                name.resize(name.size() - 4);
            names.push_back(std::move(name));
        }
        CloseHandle(snapshot);
        return names;
    }
}
