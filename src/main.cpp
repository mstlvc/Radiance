// Radiance - digital vibrance, colour, resolution and monitor control for Windows.
#include "app/App.h"
#include "platform/System.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    const radiance::LaunchOptions options = radiance::ParseLaunchOptions(GetCommandLineW());

    radiance::platform::SingleInstance single;
    if (!single.IsFirst() && !options.isolated)
    {
        // Already running: a manual launch brings the window up; the startup entry does nothing.
        if (!options.startInTray)
            radiance::platform::SingleInstance::WakeTheOtherCopy(radiance::App::kWindowClass, radiance::App::kShowMessage);
        return 0;
    }

    radiance::App app;
    return app.Run(instance, options);
}
