#include "AutoLaunch.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int)
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 2;
    int result = 0;
    HANDLE launchMutex = nullptr;
    try
    {
        // Read-only diagnostic: exit 0 = usable microphone, 1 = no usable mic.
        if (std::wstring(arguments) == L"--probe-microphone")
        {
            const bool available = auto_launch::HasUsableMicrophone();
            CoUninitialize();
            return available ? 0 : 1;
        }
        launchMutex = CreateMutexW(nullptr, TRUE, L"Local\\AllclientAutoLauncher");
        if (!launchMutex) auto_launch::Check(HRESULT_FROM_WIN32(GetLastError()));
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(launchMutex);
            CoUninitialize();
            return 0;
        }
        // Never rewrite the emulator profile while a game session owns it.
        if (HANDLE gameMutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, L"ValveHalfLifeLauncherMutex"))
        {
            const DWORD state = WaitForSingleObject(gameMutex, 0);
            if (state == WAIT_OBJECT_0 || state == WAIT_ABANDONED) ReleaseMutex(gameMutex);
            CloseHandle(gameMutex);
            if (state != WAIT_OBJECT_0 && state != WAIT_ABANDONED)
                throw std::runtime_error("Allclient is already running. Close the game before starting again.");
        }
        std::wstring executable(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!length || length >= executable.size()) throw std::runtime_error("Cannot locate Allclient folder");
        executable.resize(length);
        const auto root = std::filesystem::path(executable).parent_path();
        const auto emulatorRoot = root / L"platform/steam/games/SmartEmu";
        const auto emulator = emulatorRoot / L"SSELauncher.exe";
        if (!std::filesystem::is_regular_file(emulator) || !std::filesystem::is_regular_file(root / L"cstrike.exe"))
            throw std::runtime_error("Game or SmartEmu files are missing. Reinstall Allclient.");
        const bool voice = auto_launch::HasUsableMicrophone();
        auto_launch::ConfigureEmulator(emulatorRoot / L"config.xml", root, voice);
        if (!SetEnvironmentVariableW(L"NEXTCLIENT_AUTO_VOICE", voice ? L"1" : L"0"))
            auto_launch::Check(HRESULT_FROM_WIN32(GetLastError()));
        std::wstring command = L"\"" + emulator.wstring() + L"\" -appid 10";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(emulator.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
            nullptr, emulatorRoot.c_str(), &startup, &process))
            auto_launch::Check(HRESULT_FROM_WIN32(GetLastError()));
        CloseHandle(process.hThread);
        // Hold the launch guard until SSE has finished its handoff.
        const DWORD wait = WaitForSingleObject(process.hProcess, INFINITE);
        DWORD emulatorExit = 0;
        const BOOL gotExit = GetExitCodeProcess(process.hProcess, &emulatorExit);
        CloseHandle(process.hProcess);
        if (wait != WAIT_OBJECT_0 || !gotExit || emulatorExit != 0)
            throw std::runtime_error("SmartEmu could not complete startup. Exit code: " + std::to_string(emulatorExit));
    }
    catch (const std::exception& error)
    {
        const std::string details = error.what();
        const std::wstring message = L"Allclient could not start.\n\n" + std::wstring(details.begin(), details.end());
        MessageBoxW(nullptr, message.c_str(), L"Allclient", MB_OK | MB_ICONERROR);
        result = 2;
    }
    if (launchMutex) { ReleaseMutex(launchMutex); CloseHandle(launchMutex); }
    CoUninitialize();
    return result;
}
