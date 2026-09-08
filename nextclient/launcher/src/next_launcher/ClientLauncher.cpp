#include "ClientLauncher.h"

#include <clocale>
#include <exception>
#include <easylogging++.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <thread>
#include <mmsystem.h>

#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

#ifdef UPDATER_ENABLE
#include <updater_gui_app/updater_gui_app.h>
#endif

#include <engine_launcher_api.h>
#include <nitroapi/NitroApiInterface.h>
#include <nitro_utils/config_utils.h>
#include <nitro_utils/string_utils.h>
#include <utils/platform.h>
#include <ncl_utils/scope_exit.h>
#include <next_launcher/version.h>
#include <steam_api_proxy/next_steam_api_proxy.h>

#include "Analytics.h"
#include "DefaultUserInfo.h"
#include "RegistryUserStorage.h"
#include "EngineCommons.h"
#include "GameNetAccess.h"
#include "exception_handler.h"
#include "taskbar_icon.h"
#include "VideoSettingsDialog.h"

static const char* NITRO_API_LOG_TAG = "launcher";

namespace
{
class TimerResolutionScope
{
public:
    TimerResolutionScope() : enabled_(timeBeginPeriod(1) == TIMERR_NOERROR) { }

    ~TimerResolutionScope()
    {
        if (enabled_)
            timeEndPeriod(1);
    }

    TimerResolutionScope(const TimerResolutionScope&) = delete;
    TimerResolutionScope& operator=(const TimerResolutionScope&) = delete;

private:
    bool enabled_ = false;
};

template<class TFunc>
void RunStartupStepNoThrow(const char* step_name, TFunc&& func)
{
    try
    {
        func();
    }
    catch (const std::exception& e)
    {
        LOG(WARNING) << step_name << " failed: " << e.what();
    }
    catch (...)
    {
        LOG(WARNING) << step_name << " failed: unknown exception";
    }
}
}


ClientLauncher::ClientLauncher(HINSTANCE module_instance, const char* cmd_line) :
    module_instance_(module_instance)
{
    ProvisionDefaultConfigs();

    next_client_version_ = {
        NEXT_CLIENT_BUILD_VERSION_MAJOR,
        NEXT_CLIENT_BUILD_VERSION_MINOR,
        NEXT_CLIENT_BUILD_VERSION_PATCH,
        NEXT_CLIENT_BUILD_VERSION_PRERELEASE };

    user_storage_ = std::make_shared<RegistryUserStorage>(kNextClientRegistry);
    user_info_ = std::make_shared<DefaultUserInfo>(user_storage_);
    user_info_client_ = std::make_shared<next_launcher::UserInfoClient>(user_info_.get());
#if defined(UPDATER_ENABLE) || defined(GAMEANALYTICS_ENABLE) || defined(SENTRY_ENABLE)
    backend_address_resolver_ = std::make_shared<BackendAddressResolver>(user_info_client_);
    analytics_ = std::make_shared<Analytics>(user_info_client_, backend_address_resolver_);
#endif
    config_provider_ = std::make_shared<nitro_utils::FileConfigProvider>("user_game_config.ini");

    hl_registry_ = std::make_shared<CRegistry>(kHlRegistry);
    hl_registry_->Init();

    is_relaunch_ = GetEnvironmentVariableA(kRelaunchEnvVar, nullptr, 0) > 0;

    if (is_relaunch_)
    {
        LOG(INFO) << "Relaunch after restart";
    }
    else
    {
        RestoreGameConfigOnFreshLaunch();
    }

    InitializeCmdLine(cmd_line);

    global_win_mutex_ = CreateMutexA(nullptr, FALSE, "ValveHalfLifeLauncherMutex");
    g_SaveFullDumps = cmd_line_->CheckParm("-fulldump");
}

ClientLauncher::~ClientLauncher()
{
    ReleaseMutex(global_win_mutex_);
    CloseHandle(global_win_mutex_);
}

void ClientLauncher::ShowModuleError(const std::string& error)
{
    const std::wstring details(error.begin(), error.end());
    const std::wstring message = L"\u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc \u0641\u0627\u06cc\u0644\u200c\u0647\u0627\u06cc \u0628\u0627\u0632\u06cc \u0645\u0645\u06a9\u0646 \u0646\u0634\u062f. \u0646\u0635\u0628 \u0628\u0627\u0632\u06cc \u0631\u0627 \u0628\u0631\u0631\u0633\u06cc \u06a9\u0646\u06cc\u062f.\n\n" + details;
    MessageBoxW(nullptr, message.c_str(), L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient",
        MB_OK | MB_ICONERROR | MB_RIGHT | MB_RTLREADING | MB_DEFAULT_DESKTOP_ONLY);
}

void ClientLauncher::Run()
{
    if (!GlobalMutexCheck())
    {
        MessageBoxW(
            NULL,
            L"\u0628\u0627\u0632\u06cc \u0627\u0632 \u0642\u0628\u0644 \u062f\u0631 \u062d\u0627\u0644 \u0627\u062c\u0631\u0627\u0633\u062a.\n"
            L"\u0627\u06af\u0631 \u067e\u0646\u062c\u0631\u0647 \u0628\u0627\u0632\u06cc \u0631\u0627 \u0646\u0645\u06cc\u200c\u0628\u06cc\u0646\u06cc\u062f\u060c \u0641\u0631\u0627\u06cc\u0646\u062f \u0622\u0646 \u0631\u0627 \u062f\u0631 \u0645\u062f\u06cc\u0631\u06cc\u062a \u0648\u0638\u0627\u06cc\u0641 \u0648\u06cc\u0646\u062f\u0648\u0632 \u0628\u0628\u0646\u062f\u06cc\u062f.",
            L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient",
            MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY | MB_RIGHT | MB_RTLREADING);
        return;
    }

    LOG(INFO) << "Branch: " << user_info_client_->GetUpdateBranch();

    // Resolve the package entitlement once per launcher start. GameUI reads
    // only this result and never performs network work during gameplay.
    const GameNetAccessStatus online_access = QueryGameNetOnlineAccess();
    if (!online_access.lan_allowed)
    {
        MessageBoxW(nullptr,
            L"\u0645\u0647\u0644\u062a \u062f\u0633\u062a\u0631\u0633\u06cc \u0644\u0646 \u062a\u0645\u0627\u0645 \u0634\u062f\u0647 \u06cc\u0627 \u0647\u0646\u0648\u0632 \u0641\u0639\u0627\u0644 \u0646\u0634\u062f\u0647 \u0627\u0633\u062a.\n"
            L"\u0628\u0647 \u0627\u06cc\u0646\u062a\u0631\u0646\u062a \u0645\u062a\u0635\u0644 \u0634\u0648\u06cc\u062f \u0648 \u0628\u0627 \u0627\u0634\u062a\u0631\u0627\u06a9 \u0641\u0639\u0627\u0644 \u062f\u0648\u0628\u0627\u0631\u0647 \u0644\u0627\u0646\u0686\u0631 \u0631\u0627 \u0627\u062c\u0631\u0627 \u06a9\u0646\u06cc\u062f.\n"
            L"\u0627\u06af\u0631 \u0627\u06cc\u0646\u062a\u0631\u0646\u062a \u0645\u062a\u0635\u0644 \u0627\u0633\u062a\u060c \u062a\u0627\u0631\u06cc\u062e \u0648 \u0633\u0627\u0639\u062a \u0648\u06cc\u0646\u062f\u0648\u0632 \u0631\u0627 \u0628\u0631\u0631\u0633\u06cc \u06a9\u0646\u06cc\u062f.",
            L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient", MB_OK | MB_ICONWARNING | MB_DEFAULT_DESKTOP_ONLY | MB_RIGHT | MB_RTLREADING);
        return;
    }
    SetEnvironmentVariableA("NEXTCLIENT_ONLINE_ACCESS", online_access.allowed() ? "1" : "0");
    SetEnvironmentVariableA("NEXTCLIENT_PLAYER_NAME_TAG",
        online_access.player_name_tag.c_str());
    if (!online_access.allowed())
    {
        MessageBoxW(nullptr,
            L"\u062f\u0633\u062a\u0631\u0633\u06cc \u0622\u0646\u0644\u0627\u06cc\u0646 \u0627\u06cc\u0646 \u0646\u0633\u062e\u0647 \u0641\u0639\u0627\u0644 \u0646\u06cc\u0633\u062a. \u0628\u0627\u0632\u06cc \u062f\u0631 \u0634\u0628\u06a9\u0647 \u0645\u062d\u0644\u06cc \u062a\u0627 \u067e\u0627\u06cc\u0627\u0646 \u0645\u0647\u0644\u062a \u0644\u0646 \u062f\u0631 \u062f\u0633\u062a\u0631\u0633 \u0627\u0633\u062a.",
            L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient", MB_OK | MB_ICONWARNING | MB_DEFAULT_DESKTOP_ONLY | MB_RIGHT | MB_RTLREADING);
    }

    if (config_provider_->get_value_int("create_console_window", 0))
        CreateConsoleWindowAndRedirectOutput();

    // Video-mode changes are applied before the engine starts, avoiding the
    // fragile in-game restart path. Internal engine restarts skip this page.
    if (!is_relaunch_ && !cmd_line_->CheckParm("-novideosettings") &&
        !ShowVideoSettingsDialog(module_instance_, online_access))
        return;


    [[maybe_unused]] auto cleanup = ncl_utils::MakeScopeExit([this]
    {
        UninitializeAnalytics();
        UninitializeSentry();
    });

    RunStartupStepNoThrow("Sentry initialization", [this]
    {
        InitializeSentry();
    });

    RunStartupStepNoThrow("Analytics initialization", [this]
    {
        InitializeAnalytics();
    });

    FixScreenResolution();

    if (analytics_)
        analytics_->SendAnalyticsEvent("startup_init");

    if (analytics_)
        analytics_->SendAnalyticsEvent("startup_init_post_mutex");

    UpdaterDoneStatus updater_done = UpdaterDoneStatus::RunGame;
    RunStartupStepNoThrow("Startup updater", [this, &updater_done]
    {
        updater_done = RunStartupUpdater();
    });

    if (updater_done == UpdaterDoneStatus::RunGame)
    {
        EngineSessionResult run_result = EngineSessionResult::Exit;

        try
        {
            run_result = RunEngine();
        }
        catch (const std::exception& e)
        {
            LOG(ERROR) << "Engine startup failed: " << e.what();

            if (!is_relaunch_)
            {
                next_process_ = BuildRestartProcess();
            }
        }
        catch (...)
        {
            LOG(ERROR) << "Engine startup failed: unknown exception";

            if (!is_relaunch_)
            {
                next_process_ = BuildRestartProcess();
            }
        }

        if (run_result == EngineSessionResult::Restart)
        {
            next_process_ = BuildRestartProcess();
        }
    }

    if (updater_done == UpdaterDoneStatus::RunNewGame)
    {
        next_process_ = BuildNewGameProcess();
    }

    if (next_process_)
    {
        SetEnvironmentVariableA(kRelaunchEnvVar, "1");
    }
}

UpdaterDoneStatus ClientLauncher::RunStartupUpdater()
{
#ifdef UPDATER_ENABLE
    try
    {
        UpdaterFlags updater_flags{};
        updater_flags |= cmd_line_->CheckParm("-noupdate") ? UpdaterFlags::None : UpdaterFlags::Updater;

        auto [updater_done, available_branches] = RunUpdater(updater_flags);
        available_branches_ = available_branches;

        return updater_done;
    }
    catch (const std::exception& e)
    {
        LOG(WARNING) << "Startup updater failed: " << e.what() << ". Continuing with the game.";
        return UpdaterDoneStatus::RunGame;
    }
    catch (...)
    {
        LOG(WARNING) << "Startup updater failed: unknown exception. Continuing with the game.";
        return UpdaterDoneStatus::RunGame;
    }
#else
    return UpdaterDoneStatus::RunGame;
#endif
}

ClientLauncher::NextProcess ClientLauncher::BuildRestartProcess()
{
    std::string application = GetCurrentProcessPath().string();
    std::string command_line = cmd_line_->GetCmdLine();

    if (!cmd_line_->CheckParm("-noupdate"))
    {
        command_line += " -noupdate";
    }

    return { application, command_line };
}

ClientLauncher::NextProcess ClientLauncher::BuildNewGameProcess()
{
    std::string application = GetCurrentProcessPath()
        .filename()
        .replace_extension("")
        .string() + "_new.exe";

    std::string command_line = cmd_line_->GetCmdLine();

    if (!cmd_line_->CheckParm("-noupdate"))
    {
        command_line += " -noupdate";
    }

    return { application, command_line };
}

ClientLauncher::EngineSessionResult ClientLauncher::RunEngine()
{
    if (analytics_)
        analytics_->SendAnalyticsEvent("startup_run_engine");

    if (!is_relaunch_)
    {
        PrepareEngineCommandLine();
    }

    CheckVideoModeCrash();

    char post_restart_cmd_line[4096] = { '\0' };

    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubscribers;

    auto [nitro_api, nitro_api_module] = LoadModule<nitroapi::NitroApiInterface>("nitro_api2.dll", NITROAPI_INTERFACE_VERSION);
    if (nitro_api == nullptr)
        return EngineSessionResult::Exit;

    auto [filesystem, filesystem_module] = LoadModule<IFileSystem>("filesystem_proxy.dll", FILESYSTEM_INTERFACE_VERSION);
    if (filesystem == nullptr)
        return EngineSessionResult::Exit;

    auto [engine_mini, engine_mini_module] = LoadModule<EngineMiniInterface>("next_engine_mini.dll", ENGINE_MINI_INTERFACE_VERSION);
    if (engine_mini == nullptr)
        return EngineSessionResult::Exit;

    auto [client_mini, client_mini_module] = LoadModule<ClientMiniInterface>("cstrike\\cl_dlls\\client_mini.dll", CLIENT_MINI_INTERFACE_VERSION);
    if (client_mini == nullptr)
        return EngineSessionResult::Exit;

    auto [gameui_next, gameui_next_module] = LoadModule<IGameUINext>("cstrike\\cl_dlls\\gameui.dll", GAMEUI_NEXT_INTERFACE_VERSION);
    if (gameui_next == nullptr)
        return EngineSessionResult::Exit;

    CSysModule* steam_proxy_module = Sys_LoadModule("steam_api.dll");
    if (steam_proxy_module == nullptr)
    {
        std::string error = "Module steam_api.dll not found";

        if (analytics_)
            analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
        ShowModuleError(error);
        return EngineSessionResult::Exit;
    }

    auto steam_proxy_set_seh = (NextSteamProxy_SetSEHFunc)GetProcAddress((HMODULE)steam_proxy_module, "NextSteamProxy_SetSEH");
    if (steam_proxy_set_seh == nullptr)
    {
        std::string error = "NextSteamProxy_SetSEH not found in steam_api.dll.\n"
                            "Make sure you use the steam_api.dll from NextClient and not the original steam_api.dll";

        if (analytics_)
            analytics_->SendCrashMonitoringEvent("LoadModule Error", error.c_str(), true);
        ShowModuleError(error);
        return EngineSessionResult::Exit;
    }
    steam_proxy_set_seh(ExceptionHandler);

    filesystem->Mount();
    filesystem->AddSearchPath("", "ROOT");

    nitro_api->WriteLog(NITRO_API_LOG_TAG, sentry_init_log_.c_str());
    nitro_api->SetSEHCallback(ExceptionHandler);
    nitro_api->Initialize(cmd_line_.get(), filesystem, hl_registry_.get());

    unsubscribers.emplace_back(nitro_api->GetEngineData()->Sys_Error |= [this](const char* error, const auto& next) {
        Sys_ErrorHandler(error);
        next->Invoke(error);
    });

    unsubscribers.emplace_back(nitro_api->GetClientData()->HUD_Init += [this] {
        HUD_InitHandler();
    });

    unsubscribers.emplace_back(
        nitro_api->GetEngineData()->Sys_InitGame += [](char* lpOrgCmdLine, char* pBaseDir, void* pwnd, int bIsDedicated, bool ret) {
            if (ret)
            {
                HWND hwnd = FindCurrentProcessSDLWindow();
                if (hwnd)
                    SetTaskbarIcon(hwnd);
            }
        }
    );

    auto [engine, engine_module] = LoadModule<IEngineAPI>(kEngineDll, VENGINE_LAUNCHER_API_VERSION);
    if (engine == nullptr)
        return EngineSessionResult::Exit;

    std::string versions = CreateVersionsString(nitro_api, engine_mini, client_mini, gameui_next);
    nitro_api->WriteLog(NITRO_API_LOG_TAG, versions.c_str());

    client_mini->Init(nitro_api);
    engine_mini->Init(nitro_api, next_client_version_, analytics_.get());

    EngineCommons::Init(nitro_api, user_info_client_, versions, available_branches_);

    LOG(INFO) << "Engine command line: '" << cmd_line_->GetCmdLine() << "'";

    LOG(INFO) << "IEngineAPI::Run";
    if (analytics_)
        analytics_->AddBreadcrumb("Info", "IEngineAPI::Run");

    TimerResolutionScope timer_resolution;
    EngineRunResult engine_run_result = engine->Run(
        module_instance_,
        "",
        cmd_line_->GetCmdLine(),
        post_restart_cmd_line,
        Sys_GetFactoryThis(),
        Sys_GetFactory(filesystem_module));

    EngineCommons::Reset();

    LOG(INFO) << "IEngineAPI::Run done";
    if (analytics_)
        analytics_->AddBreadcrumb("Info", std::format("IEngineAPI::Run done, result: {}", (int)engine_run_result).c_str());

    client_mini->Uninitialize();
    Sys_UnloadModule(client_mini_module);

    engine_mini->Uninitialize();
    Sys_UnloadModule(engine_mini_module);

    Sys_UnloadModule(engine_module);
    Sys_UnloadModule(gameui_next_module);

    filesystem->Unmount();
    Sys_UnloadModule(filesystem_module);

    for (auto& unsub : unsubscribers)
        unsub->Unsubscribe();

    nitro_api->UnInitialize();
    Sys_UnloadModule(nitro_api_module);

    EngineSessionResult engine_session_result{};

    switch (engine_run_result)
    {
        case ENGRUN_QUITTING:
            engine_session_result = EngineSessionResult::Exit;
            break;

        case ENGRUN_UNSUPPORTED_VIDEOMODE:
            engine_session_result = OnVideoModeFailed() ? EngineSessionResult::Exit : EngineSessionResult::Restart;
            break;

        default:
            engine_session_result = EngineSessionResult::Restart;
            break;
    }

    config_provider_->ReloadFromFile();

    ModifyCmdLineAfterRestart(post_restart_cmd_line);

    return engine_session_result;
}

void ClientLauncher::PrepareEngineCommandLine()
{
    if (!cmd_line_->CheckParm("-game"))
        cmd_line_->AppendParm("-game", "cstrike");

    // disable glBlitFramebuffer feature, because this makes blackscreen on some Nvidia GPU when enables MSSA technology
    if (!cmd_line_->CheckParm("-nodirectblit") && !cmd_line_->CheckParm("-directblit"))
        cmd_line_->AppendParm("-nodirectblit", nullptr);

    if (!cmd_line_->CheckParm("-num_edicts"))
        cmd_line_->AppendParm("-num_edicts", "4096");
}

#ifdef UPDATER_ENABLE
UpdaterResult ClientLauncher::RunUpdater(UpdaterFlags updater_flags)
{
    LOG(INFO) << "Start the Updater GUI App";
    analytics_->SendAnalyticsEvent("startup_run_updater");

    UpdaterResult result = RunUpdaterGuiApp(
        user_info_client_,
        user_storage_,
        analytics_,
        updater_flags,
        config_provider_->get_value_string("language", "english"),
        [this](NextUpdaterEvent event) {
            analytics_->SendAnalyticsLog(AnalyticsLogType::Error, std::format("updater error, state: {}, error: {}", magic_enum::enum_name(event.state), event.error_description).c_str());
        },
        backend_address_resolver_);

    LOG(INFO) << "Updater GUI App result: " << magic_enum::enum_name(result.done_status);

    return result;
}
#endif

void ClientLauncher::CreateConsoleWindowAndRedirectOutput()
{
    AllocConsole();

    CPINFOEXA cp_info;
    if (GetCPInfoExA(CP_ACP, 0, &cp_info))
        SetConsoleOutputCP(cp_info.CodePage);

    FILE* dummy_file;
    freopen_s(&dummy_file, "CONOUT$", "w", stdout);
    freopen_s(&dummy_file, "CONOUT$", "w", stderr);
}

bool ClientLauncher::OnVideoModeFailed()
{
    hl_registry_->WriteInt("ScreenBPP", 32);
    hl_registry_->WriteInt("ScreenWidth", kDefaultWidth);
    hl_registry_->WriteInt("ScreenHeight", kDefaultHeight);
    hl_registry_->WriteString("EngineDLL", "hw.dll");

    return MessageBoxW(
        NULL,
        L"\u062d\u0627\u0644\u062a \u0646\u0645\u0627\u06cc\u0634 \u0627\u0646\u062a\u062e\u0627\u0628\u200c\u0634\u062f\u0647 \u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u06cc \u0646\u0645\u06cc\u200c\u0634\u0648\u062f.\n"
        L"\u0628\u0627\u0632\u06cc \u062f\u0648\u0628\u0627\u0631\u0647 \u0627\u062c\u0631\u0627 \u0634\u0648\u062f\u061f",
        L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient",
        MB_OKCANCEL | MB_ICONERROR | MB_ICONQUESTION | MB_DEFAULT_DESKTOP_ONLY | MB_RIGHT | MB_RTLREADING) == IDOK;
}

void ClientLauncher::FixScreenResolution()
{
    int w = hl_registry_->ReadInt("ScreenWidth", kDefaultWidth);
    int h = hl_registry_->ReadInt("ScreenHeight", kDefaultHeight);

    if (w < kMinWidth || h < kMinHeight)
    {
        hl_registry_->WriteInt("ScreenWidth", kDefaultWidth);
        hl_registry_->WriteInt("ScreenHeight", kDefaultHeight);
    }
}

bool ClientLauncher::GlobalMutexCheck()
{
    DWORD result = WaitForSingleObject(global_win_mutex_, 0);
    if (result != WAIT_OBJECT_0 &&
        result != WAIT_ABANDONED &&
        !cmd_line_->CheckParm("-hijack"))
    {
        return false;
    }

    return true;
}

#ifdef GAMEANALYTICS_ENABLE

void ClientLauncher::InitializeAnalytics()
{
    try
    {
        std::string client_uid = user_info_client_->GetClientUid();

        gameanalytics::GameAnalytics::setEnabledErrorReporting(false);
        gameanalytics::GameAnalytics::disableDeviceInfo();
        gameanalytics::GameAnalytics::configureUserId(client_uid.c_str());
        gameanalytics::GameAnalytics::configureBuild(NEXT_CLIENT_BUILD_VERSION);
        gameanalytics::GameAnalytics::initialize(GAMEANALYTICS_GAME_KEY, GAMEANALYTICS_SECRET_KEY);

        analytics_->SendBranchEvent();
        analytics_->SendPrimaryBackendEvent();
        analytics_->SendActualBackendEvent();
    }
    catch (const std::exception& e)
    {
        LOG(WARNING) << "Analytics initialization failed: " << e.what();
        analytics_.reset();
    }
    catch (...)
    {
        LOG(WARNING) << "Analytics initialization failed: unknown exception";
        analytics_.reset();
    }
}

void ClientLauncher::UninitializeAnalytics()
{
    if (analytics_)
        gameanalytics::GameAnalytics::onQuit();
}

#else

void ClientLauncher::InitializeAnalytics() { }
void ClientLauncher::UninitializeAnalytics() { }

#endif

#ifdef SENTRY_ENABLE

void ClientLauncher::InitializeSentry()
{
    try
    {
        CHAR exe_file_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_file_path, MAX_PATH);

        std::string client_uid = user_info_client_->GetClientUid();

        sentry_options_t *options = sentry_options_new();
        sentry_options_set_dsn(options, SENTRY_UPLOAD_URL);
        sentry_options_set_handler_path(options, "crashpad_handler.exe");
        sentry_options_set_database_path(options, "crashes");
        sentry_options_set_release(options, SENTRY_PROJECT_NAME "@" NEXT_CLIENT_BUILD_VERSION);
        sentry_options_set_environment(options, SENTRY_ENV);
        sentry_options_set_max_breadcrumbs(options, 100);

        sentry_options_add_attachment(options, "nitro_api.log");
        sentry_options_add_attachment(options, "setting_guard.ini");
        sentry_options_add_attachment(options, "user_game_config.ini");
        sentry_options_add_attachment(options, "launcher.log");
        sentry_options_add_attachment(options, "platform\\config\\backend.json");

        int result = sentry_init(options);

        sentry_value_t user = sentry_value_new_object();
        sentry_value_set_by_key(user, "id", sentry_value_new_string(client_uid.c_str()));
        sentry_value_set_by_key(user, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_value_set_by_key(user, "Game Dir", sentry_value_new_string(exe_file_path));
        sentry_set_user(user);

        if (result == 0)
            sentry_init_log_ = "Sentry started";
        else
            sentry_init_log_ = std::format("Sentry error: {}", GetWinErrorString(GetLastError()));
    }
    catch (const std::exception& e)
    {
        sentry_init_log_ = std::format("Sentry error: {}", e.what());
        LOG(WARNING) << sentry_init_log_;
    }
    catch (...)
    {
        sentry_init_log_ = "Sentry error: unknown exception";
        LOG(WARNING) << sentry_init_log_;
    }

}

void ClientLauncher::UninitializeSentry()
{
    sentry_close();
}

#else

void ClientLauncher::InitializeSentry() { }
void ClientLauncher::UninitializeSentry() { }

#endif

void ClientLauncher::HUD_InitHandler()
{
    if (analytics_)
        analytics_->SendAnalyticsEvent("startup_hud_init_post");
}

void ClientLauncher::InitializeCmdLine(const char* cmd_line)
{
    cmd_line_ = std::make_shared<CCommandLine>();

    if (is_relaunch_)
    {
        cmd_line_->CreateCmdLine(cmd_line);
        return;
    }

    std::string launch_parameters = config_provider_->get_value_string("launch_parameters", "");
    cmd_line_->CreateCmdLine(std::format("{} {}", cmd_line, launch_parameters).c_str());

    if (config_provider_->get_value_int("stretch_aspect", 0))
    {
        cmd_line_->AppendParm("-stretchaspect", nullptr);
    }
}

void ClientLauncher::ModifyCmdLineAfterRestart(const char* cmd_line)
{
    static const char* kRemoveParams[] = {
        "-sw",
        "-startwindowed",
        "-windowed",
        "-window",
        "-full",
        "-fullscreen",
        "-soft",
        "-software",
        "-gl",
        "-w",
        "-width",
        "-h",
        "-height",
        "+connect"
    };

    for (const auto& param : kRemoveParams)
    {
        cmd_line_->RemoveParm(param);
    }

    if (strstr(cmd_line, "-game"))
    {
        cmd_line_->RemoveParm("-game");
    }

    if (strstr(cmd_line, "+load"))
    {
        cmd_line_->RemoveParm("+load");
    }

    if (config_provider_->get_value_int("stretch_aspect", 0))
    {
        cmd_line_->AppendParm("-stretchaspect", nullptr);
    }
    else
    {
        cmd_line_->RemoveParm("-stretchaspect");
    }

    cmd_line_->SetParm("-novid", nullptr);

    cmd_line_->AppendParm(cmd_line, nullptr);
}

void ClientLauncher::CheckVideoModeCrash()
{
    if (hl_registry_->ReadInt("CrashInitializingVideoMode", 0) == 0)
    {
        return;
    }

    hl_registry_->WriteInt("CrashInitializingVideoMode", 0);

    if (MessageBoxW(
        NULL,
        L"\u0627\u062c\u0631\u0627\u06cc \u0642\u0628\u0644\u06cc \u0628\u0627\u0632\u06cc \u0628\u0647 \u062f\u0644\u06cc\u0644 \u062e\u0637\u0627\u06cc \u06af\u0631\u0627\u0641\u06cc\u06a9\u06cc \u0646\u0627\u0645\u0648\u0641\u0642 \u0628\u0648\u062f\u0647 \u0627\u0633\u062a.\n"
        L"\u0648\u0636\u0648\u062d \u062a\u0635\u0648\u06cc\u0631 \u0628\u0627\u0632\u0646\u0634\u0627\u0646\u06cc \u0648 \u0628\u0627\u0632\u06cc \u062f\u0648\u0628\u0627\u0631\u0647 \u0627\u062c\u0631\u0627 \u0634\u0648\u062f\u061f",
        L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient",
        MB_OKCANCEL | MB_ICONERROR | MB_ICONQUESTION | MB_DEFAULT_DESKTOP_ONLY | MB_RIGHT | MB_RTLREADING) != IDOK)
    {
        return;
    }

    hl_registry_->WriteInt("ScreenBPP", 32);
    hl_registry_->WriteInt("ScreenWidth", kDefaultWidth);
    hl_registry_->WriteInt("ScreenHeight", kDefaultHeight);
}

void ClientLauncher::Sys_ErrorHandler(const char* error)
{
    if (analytics_)
        analytics_->SendCrashMonitoringEvent("Sys_Error", error, true);

    // Sys_Error exits from inside IEngineAPI::Run, so the teardown after it never happens.
    EngineCommons::Reset();
}

std::string ClientLauncher::CreateVersionsString(nitroapi::NitroApiInterface* nitro_api,
                                                 EngineMiniInterface* engine_mini,
                                                 ClientMiniInterface* client_mini,
                                                 IGameUINext* gameui_next)
{
    char nitroapi_version[64]; nitro_api->GetVersion(nitroapi_version, sizeof(nitroapi_version));
    char engine_mini_version[64]; engine_mini->GetVersion(engine_mini_version, sizeof(engine_mini_version));
    char client_mini_version[64]; client_mini->GetVersion(client_mini_version, sizeof(client_mini_version));
    char gameui_version[64]; gameui_next->GetVersion(nullptr, nullptr, nullptr, gameui_version, sizeof(gameui_version));

    char versions[512];
    V_snprintf(versions,
               sizeof(versions),
               "nitro_api: %s\nLauncher: %s\nnext_engine_mini: %s\nclient_mini: %s\nGameUI: %s\n",
               nitroapi_version, LAUNCHER_VERSION, engine_mini_version, client_mini_version, gameui_version);

    return versions;
}

void ClientLauncher::ProvisionDefaultConfigs()
{
    namespace fs = std::filesystem;

    for (const char* target : kDefaultConfigs)
    {
        fs::path target_path(target);
        fs::path default_path = fs::path(kDefaultConfigsDir) / target_path.filename();

        std::error_code ec;
        if (fs::exists(target_path, ec))
        {
            continue;
        }

        if (!fs::exists(default_path, ec))
        {
            LOG(WARNING) << "Default config not found: " << default_path.string();
            continue;
        }

        if (fs::path parent = target_path.parent_path(); !parent.empty())
        {
            fs::create_directories(parent, ec);
        }

        if (fs::copy_file(default_path, target_path, ec))
        {
            LOG(INFO) << "Provisioned config from default: " << target;
        }
        else
        {
            LOG(WARNING) << "Failed to provision config '" << target << "': " << ec.message();
        }
    }
}

void ClientLauncher::RestoreGameConfigOnFreshLaunch()
{
    namespace fs = std::filesystem;

    constexpr char kDefaultConfig[] = "default/config.cfg";
    constexpr char kGameConfig[] = "cstrike/config.cfg";

    std::error_code ec;
    if (!fs::is_regular_file(kDefaultConfig, ec))
        return;

    ec.clear();
    fs::create_directories(fs::path(kGameConfig).parent_path(), ec);
    if (ec)
        return;

    ec.clear();
    fs::copy_file(kDefaultConfig, kGameConfig, fs::copy_options::overwrite_existing, ec);
}
