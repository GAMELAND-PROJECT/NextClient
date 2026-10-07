#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>
#include <steam_api_proxy/next_steam_api_proxy.h>
#include "SteamUserProxy.h"
#include "SteamEmu.h"
#include "SteamStubs.h"
#include "../voice/VoiceRecorder.h"

#ifdef _WINDOWS
#include <Windows.h>
#include <nitro_utils/platform.h>
#else
#include <climits>
#include <iostream>
#include <signal.h>
#include <execinfo.h>
#include <nitroapi/NitroApiInterface.h>
#include <client_mini/client_mini.h>
#undef GetProcAddress
#include <nitro_utils/platform.h>
#endif

static NextSteamProxy_ExceptionFunc g_ExceptionCallback = nullptr;
static bool g_bInitialized = false;

#ifndef _WINDOWS
void SigHandler(int signum)
{
    std::cerr << "Crash! signal " << signum << "\n";
    std::cerr << "See backtrace in crash_backtrace.txt\n";

    void* array[100];
    size_t size = backtrace(array, sizeof(array));
    char** strings = backtrace_symbols(array, size);

    std::ofstream outfile("crash_backtrace.txt", std::ios_base::app);
    if (outfile.is_open())
    {
        outfile << "Signal " << signum << "\n";
        if (strings != nullptr)
        {
            for (size_t i = 0; i < size; i++) {
                outfile << strings[i] << "\n";
            }
        }
        outfile.close();
    }

    fflush(stdout);
    fflush(stderr);

    signal(signum, SIG_DFL);
    exit(3);
}

void SetSigHandlers()
{
    signal(SIGSEGV, SigHandler);
    signal(SIGFPE, SigHandler);
    signal(SIGILL, SigHandler);
    signal(SIGABRT, SigHandler);
}
#endif

#ifdef _WINDOWS
static HMODULE g_hSmartSteamEmu = nullptr;

static void TryLoadSmartSteamEmu()
{
    if (g_hSmartSteamEmu)
        return;

    const wchar_t* candidates[] = {
        L"platform\\steam\\games\\SmartEmu\\SmartSteamEmu\\SmartSteamEmu.dll",
        L"platform\\steam\\games\\SmartEmu2\\SmartSteamEmu\\SmartSteamEmu.dll",
        L"SmartSteamEmu.dll"
    };

    for (const wchar_t* path : candidates)
    {
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        {
            // Sync SteamID to SmartSteamEmu.ini and launcher.ini so SSE uses the exact same SteamID as our server auth ticket
            uint64_t fullSteamId = SteamEmu::GetSteamID().ConvertToUint64();
            wchar_t iniPath[MAX_PATH] = { 0 };
            wcscpy_s(iniPath, path);
            wchar_t* lastSlash = wcsrchr(iniPath, L'\\');
            if (lastSlash)
            {
                *(lastSlash + 1) = L'\0';
                std::wstring sseIni = std::wstring(iniPath) + L"SmartSteamEmu.ini";
                std::wstring launcherIni = std::wstring(iniPath) + L"launcher.ini";

                wchar_t idBuf[32];
                swprintf_s(idBuf, L"%llu", fullSteamId);

                WritePrivateProfileStringW(L"SmartSteamEmu", L"SteamIdGeneration", L"Manual", sseIni.c_str());
                WritePrivateProfileStringW(L"SmartSteamEmu", L"ManualSteamId", idBuf, sseIni.c_str());
                WritePrivateProfileStringW(L"SmartSteamEmu", L"EnableInGameVoice", L"1", sseIni.c_str());

                WritePrivateProfileStringW(L"SmartSteamEmu", L"SteamIdGeneration", L"Manual", launcherIni.c_str());
                WritePrivateProfileStringW(L"SmartSteamEmu", L"ManualSteamId", idBuf, launcherIni.c_str());
                WritePrivateProfileStringW(L"SmartSteamEmu", L"EnableInGameVoice", L"1", launcherIni.c_str());
            }

            g_hSmartSteamEmu = LoadLibraryW(path);
            if (g_hSmartSteamEmu)
            {
                typedef bool (*SteamAPI_Init_Fn)();
                typedef ISteamUser* (*SteamUser_Fn)();

                SteamAPI_Init_Fn pInit = (SteamAPI_Init_Fn)GetProcAddress(g_hSmartSteamEmu, "SteamAPI_Init");
                SteamUser_Fn pUser = (SteamUser_Fn)GetProcAddress(g_hSmartSteamEmu, "SteamUser");

                if (pInit)
                    pInit();

                if (pUser)
                {
                    ISteamUser* sseUser = pUser();
                    if (sseUser)
                    {
                        g_SteamUserVoiceProxy.SetOriginal(sseUser);
                    }
                }
                break;
            }
        }
    }
}
#endif

void Initialize()
{
    if (g_bInitialized)
        return;

    g_bInitialized = true;
    SteamEmu::Initialize();

#ifdef _WINDOWS
    TryLoadSmartSteamEmu();
#endif

#ifndef _WINDOWS
    remove("crash_backtrace.txt");
    SetSigHandlers();

    CSysModule* nitro_module = Sys_LoadModule("nitro_api2.so");
    CreateInterfaceFn nitro_factory = Sys_GetFactory(nitro_module);
    if (nitro_factory)
    {
        nitroapi::NitroApiInterface* nitroapi = (nitroapi::NitroApiInterface*)nitro_factory(NITROAPI_INTERFACE_VERSION, nullptr);
        CSysModule* client_mini_module = Sys_LoadModule("cstrike/cl_dlls/client_mini.so");
        CreateInterfaceFn client_mini_factory = Sys_GetFactory(client_mini_module);
        if (client_mini_factory)
        {
            ClientMiniInterface* client_mini = (ClientMiniInterface*)client_mini_factory(CLIENT_MINI_INTERFACE_VERSION, nullptr);
            nitroapi->Initialize(nullptr, nullptr, nullptr);
            client_mini->Init(nitroapi);
        }
    }
#endif
}

void UnInitialize()
{
    if (!g_SteamUserVoiceProxy.GetOriginal())
    {
        VoiceRecorder::GetInstance().Shutdown();
    }
    g_bInitialized = false;
    g_ExceptionCallback = nullptr;
}


bool IsInitialized()
{
    return g_bInitialized;
}

S_API void NextSteamProxy_SetSEH(NextSteamProxy_ExceptionFunc exception_callback)
{
    g_ExceptionCallback = exception_callback;
}

S_API bool SteamAPI_Init()
{
    if (!IsInitialized())
        Initialize();

    return true;
}

S_API void SteamAPI_Shutdown()
{
    UnInitialize();
}

S_API void SteamAPI_RunCallbacks()
{
    if (!IsInitialized())
        Initialize();
}

S_API void SteamAPI_RegisterCallback(class CCallbackBase* pCallback, int iCallback)
{
    (void)pCallback;
    (void)iCallback;
    if (!IsInitialized())
        Initialize();
}

S_API void SteamAPI_UnregisterCallback(class CCallbackBase* pCallback)
{
    (void)pCallback;
    if (!IsInitialized())
        Initialize();
}

S_API void SteamAPI_RegisterCallResult(class CCallbackBase* pCallback, SteamAPICall_t hAPICall)
{
    (void)pCallback;
    (void)hAPICall;
    if (!IsInitialized())
        Initialize();
}

S_API void SteamAPI_UnregisterCallResult(class CCallbackBase* pCallback, SteamAPICall_t hAPICall)
{
    (void)pCallback;
    (void)hAPICall;
    if (!IsInitialized())
        Initialize();
}

S_API void SteamAPI_UseBreakpadCrashHandler(char const* pchVersion, char const* pchDate, char const* pchTime, bool bFullMemoryDumps, void* pvContext, PFNPreMinidumpCallback m_pfnPreMinidumpCallback)
{
    (void)pchVersion;
    (void)pchDate;
    (void)pchTime;
    (void)bFullMemoryDumps;
    (void)pvContext;
    (void)m_pfnPreMinidumpCallback;
}

S_API void SteamAPI_SetBreakpadAppID(uint32 unAppID)
{
    (void)unAppID;
    if (!IsInitialized())
        Initialize();
}

S_API void SteamAPI_WriteMiniDump(uint32 uStructuredExceptionCode, void* pvExceptionInfo, uint32 uBuildID)
{
    (void)uStructuredExceptionCode;
    (void)pvExceptionInfo;
    (void)uBuildID;
}

S_API void SteamAPI_SetMiniDumpComment(const char* pchMsg)
{
    (void)pchMsg;
    if (!IsInitialized())
        Initialize();
}

S_API ISteamUser *SteamUser()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamUserVoiceProxy;
}

S_API ISteamFriends *SteamFriends()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamFriendsStub;
}

S_API ISteamUtils *SteamUtils()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamUtilsStub;
}

S_API ISteamMatchmaking *SteamMatchmaking()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamMatchmakingStub;
}

S_API ISteamUserStats *SteamUserStats()
{
    return nullptr;
}

S_API ISteamApps *SteamApps()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamAppsStub;
}

S_API ISteamNetworking *SteamNetworking()
{
    return nullptr;
}

S_API ISteamMatchmakingServers *SteamMatchmakingServers()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamMatchmakingServersStub;
}

S_API ISteamRemoteStorage *SteamRemoteStorage()
{
    return nullptr;
}

S_API ISteamScreenshots *SteamScreenshots()
{
    return nullptr;
}

S_API ISteamHTTP *SteamHTTP()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamHTTPStub;
}

S_API ISteamUnifiedMessages *SteamUnifiedMessages()
{
    return nullptr;
}

S_API bool SteamGameServer_Init(uint32 unIP, uint16 usSteamPort, uint16 usGamePort, uint16 usQueryPort, EServerMode eServerMode, const char* pchVersionString)
{
    (void)unIP;
    (void)usSteamPort;
    (void)usGamePort;
    (void)usQueryPort;
    (void)eServerMode;
    (void)pchVersionString;

    if (!IsInitialized())
        Initialize();

    return true;
}

S_API void SteamGameServer_Shutdown()
{
}

S_API void SteamGameServer_RunCallbacks()
{
}

S_API ISteamGameServer* SteamGameServer()
{
    if (!IsInitialized())
        Initialize();

    return &g_SteamGameServerStub;
}
