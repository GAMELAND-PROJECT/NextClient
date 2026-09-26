#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <thread>

namespace gameland_guard
{
    // Initializes the internal standalone anti-cheat engine
    void Initialize();

    // Runs pre-launch scans (proxy DLLs, debuggers, blacklisted processes)
    bool PreLaunchScan();

    // Starts the background watchdog thread after engine window creation
    void StartWatchdog(HWND gameWindow);

    // Stops the watchdog thread cleanly upon game exit
    void StopWatchdog();

    // Triggered upon violation discovery
    void OnViolation(const wchar_t* violationType, const wchar_t* details);
}
