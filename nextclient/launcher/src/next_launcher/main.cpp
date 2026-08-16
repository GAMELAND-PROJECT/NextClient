#include <Windows.h>

#include <format>
#include <optional>
#include <string>

#include <easylogging++.h>
#include <next_launcher/version.h>
#include <ncl_utils/safe_result.h>
#include <taskcoro/TaskCoro.h>
#include <taskcoro/impl/TaskCoroImpl.h>
#include <utils/platform.h>

#include "ClientLauncher.h"

INITIALIZE_EASYLOGGINGPP

using namespace ncl_utils;

namespace
{
    std::shared_ptr<taskcoro::TaskCoroImpl> g_TaskCoroImpl;

    void SetupLogger()
    {
        el::Configurations config;
        config.setToDefault();
        config.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, std::to_string(MAX_LOGFILE_SIZE));
        config.set(el::Level::Global, el::ConfigurationType::Format, "%datetime [%level] %msg");
        el::Loggers::reconfigureLogger(el::base::consts::kDefaultLoggerId, config);
    }

    void SetupLocale()
    {
        setlocale(LC_CTYPE, "");
        setlocale(LC_TIME, "");
        setlocale(LC_COLLATE, "");
        setlocale(LC_MONETARY, "");
    }

    void SetupTaskCoro()
    {
        g_TaskCoroImpl = std::make_shared<taskcoro::TaskCoroImpl>(std::this_thread::get_id(), false);
        taskcoro::TaskCoro::Initialize(g_TaskCoroImpl);
    }

    void ReleaseTaskCoro()
    {
        taskcoro::TaskCoro::UnInitialize();
        g_TaskCoroImpl = nullptr;
    }

    void SpawnProcess(const std::string& application, std::string command_line)
    {
        PROCESS_INFORMATION process_information;
        STARTUPINFOA startupinfo;
        ZeroMemory(&startupinfo, sizeof(startupinfo));
        startupinfo.cb = sizeof(startupinfo);

        bool result = CreateProcessA(
            application.c_str(),
            command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &startupinfo,
            &process_information
        );

        if (result)
        {
            CloseHandle(process_information.hThread);
            CloseHandle(process_information.hProcess);
        }
        else
        {
            LOG(ERROR) << "Can't CreateProcessA: " << application << ". Error: " << GetWinErrorString(GetLastError());
        }
    }
} // namespace


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow)
{
    SetupLogger();

    LOG(INFO) << "-----------------------------------------------";
    LOG(INFO) << "Start " << GetCurrentProcessPath().filename();
    LOG(INFO) << "Version: " << NEXT_CLIENT_BUILD_VERSION;

    SetupTaskCoro();
    SetupLocale();

    std::optional<ClientLauncher::NextProcess> next_process;
    {
        auto launcher = std::make_unique<ClientLauncher>(hInstance, GetCommandLineA());
        launcher->Run();

        next_process = launcher->next_process();
    }

    ReleaseTaskCoro();

    LOG(INFO) << "Exit";

    if (next_process)
    {
        el::Loggers::flushAll();
        SpawnProcess(next_process->application, next_process->command_line);
    }

    return EXIT_SUCCESS;
}
