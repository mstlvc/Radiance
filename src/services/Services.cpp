#include "services/Services.h"

namespace radiance
{
    Services MakeServices()
    {
        Services s;
        s.colour = MakeColourEngine();
        s.displays = MakeDisplayService();
        s.monitors = MakeMonitorControl();
        s.games = MakeGameWatcher();
        return s;
    }
}
