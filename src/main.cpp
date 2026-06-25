#define WIN32_LEAN_AND_MEAN

#include "ClipboardMonitorApp.h"

#include <windows.h>

int wmain()
{
    return ojfix::RunClipboardMonitor();
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int showCommand)
{
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(commandLine);
    UNREFERENCED_PARAMETER(showCommand);

    return ojfix::RunClipboardMonitor();
}
