#include "main.h"
#ifdef _WIN32
#include <direct.h>
#endif
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <ranges>
#include <string>
#include <string_view>
#include <next_client_mini/client_mini.h>
#include <parsemsg.h>
#include <demo_api.h>

#include "camera.h"
#include "studiorenderer.h"
#include "view.h"
#include "fov.h"
#include "color_chat_in_console.h"
#include "inspect.h"
#include "invert_mouse.h"

nitroapi::NitroApiInterface* g_NitroApi;

IGameConsole* g_GameConsole;
IGameConsoleNext* g_GameConsoleNext;

cldll_func_t cl_funcs;
cl_enginefunc_t gEngfuncs;
enginefuncs_t g_engfuncs;
local_state_t g_LastPlayerState;
client_data_t g_LastClientData;
server_t* sv;
server_static_t* sv_static;

gamehud_t* gHUD;
int g_iUser1;
int g_iUser2;
int g_CurrentWeaponId;

engine_studio_api_t IEngineStudio;
r_studio_interface_t g_OriginalStudio;
playermove_t* pmove;

std::unique_ptr<GameHud> g_GameHud;
static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsub;
static bool g_MouseCaptureKnown = false;
static bool g_MouseCaptured = false;
static dlight_t* (*g_OriginalAllocDlight)(int) = nullptr;
static dlight_t* (*g_OriginalAllocElight)(int) = nullptr;

namespace
{
    bool g_InputBackground = false;
    bool g_DemoMenuVisible = false;
    enum class DemoMenuAction
    {
        None,
        Start,
        Stop,
    };
    DemoMenuAction g_PendingDemoAction = DemoMenuAction::None;
    dlight_t g_SuppressedDlight{};
    double g_AnonymousDlightWindowStarted = -1.0;
    unsigned int g_AnonymousDlightsInWindow = 0;

    dlight_t* GuardedAllocLight(int key, dlight_t* (*original)(int))
    {
        if (original == nullptr || key != 0)
            return original != nullptr ? original(key) : &g_SuppressedDlight;

        // The stock renderer has a small fixed light pool. Eight anonymous
        // lights per 10 ms window retain simultaneous muzzle flashes while
        // preventing effect-heavy servers from churning the whole pool.
        constexpr unsigned int kMaxAnonymousDlightsPerFrameWindow = 8;
        constexpr double kDlightFrameWindow = 0.010;

        const double now = gEngfuncs.GetClientTime();
        if (g_AnonymousDlightWindowStarted < 0.0 || now < g_AnonymousDlightWindowStarted ||
            now - g_AnonymousDlightWindowStarted >= kDlightFrameWindow)
        {
            g_AnonymousDlightWindowStarted = now;
            g_AnonymousDlightsInWindow = 0;
        }

        if (g_AnonymousDlightsInWindow >= kMaxAnonymousDlightsPerFrameWindow)
        {
            Q_memset(&g_SuppressedDlight, 0, sizeof(g_SuppressedDlight));
            return &g_SuppressedDlight;
        }

        ++g_AnonymousDlightsInWindow;
        return original(key);
    }

    dlight_t* GuardedAllocDlight(int key)
    {
        return GuardedAllocLight(key, g_OriginalAllocDlight);
    }

    dlight_t* GuardedAllocElight(int key)
    {
        return GuardedAllocLight(key, g_OriginalAllocElight);
    }

    void InstallDynamicLightGuard()
    {
        if (gEngfuncs.pEfxAPI == nullptr || g_OriginalAllocDlight != nullptr)
            return;

        g_OriginalAllocDlight = gEngfuncs.pEfxAPI->CL_AllocDlight;
        g_OriginalAllocElight = gEngfuncs.pEfxAPI->CL_AllocElight;
        gEngfuncs.pEfxAPI->CL_AllocDlight = GuardedAllocDlight;
        gEngfuncs.pEfxAPI->CL_AllocElight = GuardedAllocElight;
    }

    void RemoveDynamicLightGuard()
    {
        if (gEngfuncs.pEfxAPI != nullptr)
        {
            if (g_OriginalAllocDlight != nullptr && gEngfuncs.pEfxAPI->CL_AllocDlight == GuardedAllocDlight)
                gEngfuncs.pEfxAPI->CL_AllocDlight = g_OriginalAllocDlight;
            if (g_OriginalAllocElight != nullptr && gEngfuncs.pEfxAPI->CL_AllocElight == GuardedAllocElight)
                gEngfuncs.pEfxAPI->CL_AllocElight = g_OriginalAllocElight;
        }

        g_OriginalAllocDlight = nullptr;
        g_OriginalAllocElight = nullptr;
        g_AnonymousDlightWindowStarted = -1.0;
        g_AnonymousDlightsInWindow = 0;
    }

    bool IsInputBackground()
    {
        return g_InputBackground;
    }

    bool BindingEquals(const char* binding, std::string_view expected)
    {
        return binding != nullptr && expected == binding;
    }

    bool IsDemoMenuKey(int keynum)
    {
        constexpr int kF4 = 138;
        return keynum == kF4;
    }

    void ShowDemoMenu()
    {
        g_DemoMenuVisible = true;
    }

    void ToggleDemoMenu()
    {
        g_DemoMenuVisible = !g_DemoMenuVisible;
    }

    void ShowDemoMenuCommand()
    {
        ToggleDemoMenu();
    }

    void HideDemoMenu()
    {
        g_DemoMenuVisible = false;
    }

    bool IsClientDemoRecording()
    {
        if (gEngfuncs.pDemoAPI != nullptr && gEngfuncs.pDemoAPI->IsRecording != nullptr)
        {
            if (gEngfuncs.pDemoAPI->IsRecording())
                return true;
        }

        if (eng() != nullptr && eng()->client_static != nullptr)
        {
            if (eng()->client_static->demorecording)
                return true;
        }

        return false;
    }

    std::string GetCurrentRecordingDemoName()
    {
        if (eng() != nullptr && eng()->client_static != nullptr && eng()->client_static->demorecording)
        {
            if (eng()->client_static->demofilename[0] != '\0')
            {
                std::string fname = eng()->client_static->demofilename;
                const size_t slash = fname.find_last_of("/\\");
                if (slash != std::string::npos)
                    fname.erase(0, slash + 1);
                return fname;
            }
        }
        return "active";
    }

    void DrawHudString(int x, int y, const char* text)
    {
        if (text != nullptr)
            gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
    }

    void DrawHudBox(int x, int y, int w, int h, int r, int g, int b, int a)
    {
        if (gEngfuncs.pfnFillRGBA != nullptr)
            gEngfuncs.pfnFillRGBA(x, y, w, h, r, g, b, a);
    }

    void DrawDemoMenu()
    {
        const bool isRecording = IsClientDemoRecording();

        const int menuX = 35;
        const int menuY = 130;
        const int menuW = 285;
        const int menuH = 175;

        // Background panel (Dark translucent CS-style)
        DrawHudBox(menuX, menuY, menuW, menuH, 12, 16, 20, 215);

        // Header accent bar (Gold / Amber)
        DrawHudBox(menuX, menuY, menuW, 3, 255, 178, 28, 255);

        // Border outline
        DrawHudBox(menuX, menuY, menuW, 1, 60, 75, 90, 160);
        DrawHudBox(menuX, menuY + menuH - 1, menuW, 1, 60, 75, 90, 160);
        DrawHudBox(menuX, menuY, 1, menuH, 60, 75, 90, 160);
        DrawHudBox(menuX + menuW - 1, menuY, 1, menuH, 60, 75, 90, 160);

        // Header Title
        gEngfuncs.pfnDrawSetTextColor(1.0f, 0.78f, 0.12f);
        DrawHudString(menuX + 16, menuY + 12, "GAMELAND DEMO RECORDER");

        // Separator line
        DrawHudBox(menuX + 12, menuY + 34, menuW - 24, 1, 70, 85, 100, 120);

        // Status section
        gEngfuncs.pfnDrawSetTextColor(0.70f, 0.75f, 0.80f);
        DrawHudString(menuX + 16, menuY + 44, "STATUS:");

        if (isRecording)
        {
            // Vivid green pulse indicator
            gEngfuncs.pfnDrawSetTextColor(0.15f, 1.0f, 0.25f);
            DrawHudString(menuX + 75, menuY + 44, "[*] RECORDING IN PROGRESS");

            const std::string curDemo = GetCurrentRecordingDemoName();
            char demoInfo[96]{};
            std::snprintf(demoInfo, sizeof(demoInfo), "File: %s", curDemo.c_str());
            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.80f, 0.60f);
            DrawHudString(menuX + 16, menuY + 62, demoInfo);
        }
        else
        {
            // Dim standby indicator
            gEngfuncs.pfnDrawSetTextColor(0.95f, 0.35f, 0.20f);
            DrawHudString(menuX + 75, menuY + 44, "[o] STANDBY / IDLE");

            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.60f, 0.65f);
            DrawHudString(menuX + 16, menuY + 62, "Ready to capture current match.");
        }

        // Sub separator
        DrawHudBox(menuX + 12, menuY + 84, menuW - 24, 1, 55, 65, 75, 100);

        // Menu Option 1
        if (isRecording)
        {
            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.55f, 0.55f);
            DrawHudString(menuX + 16, menuY + 95, "1. Start New Demo");
        }
        else
        {
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.82f, 0.20f);
            DrawHudString(menuX + 16, menuY + 95, "1. Start Demo Recording");
        }

        // Menu Option 2
        if (isRecording)
        {
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.30f, 0.30f);
            DrawHudString(menuX + 16, menuY + 118, "2. Stop Demo Recording");
        }
        else
        {
            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.55f, 0.55f);
            DrawHudString(menuX + 16, menuY + 118, "2. Stop Demo (Inactive)");
        }

        // Bottom separator
        DrawHudBox(menuX + 12, menuY + 142, menuW - 24, 1, 55, 65, 75, 100);

        // Menu Option 0 / F4 Close
        gEngfuncs.pfnDrawSetTextColor(0.75f, 0.78f, 0.82f);
        DrawHudString(menuX + 16, menuY + 151, "0. Close Menu  (Press F4)");
    }

    std::string TrimExtension(std::string value, std::string_view extension)
    {
        if (value.size() >= extension.size())
        {
            const std::string_view tail(value.data() + value.size() - extension.size(), extension.size());
            bool matches = true;
            for (size_t i = 0; i < extension.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(tail[i])) !=
                    std::tolower(static_cast<unsigned char>(extension[i])))
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
                value.resize(value.size() - extension.size());
        }

        return value;
    }

    std::string SanitizeDemoPart(const char* rawValue, const char* fallback)
    {
        std::string result;
        for (const unsigned char ch : std::string(rawValue != nullptr ? rawValue : ""))
        {
            if (std::isalnum(ch) || ch == '_' || ch == '-')
                result.push_back(static_cast<char>(ch));
            else if (ch == ' ' || ch == '/' || ch == '\\' || ch == ':' || ch == '.')
                result.push_back('_');
        }

        while (!result.empty() && result.front() == '_')
            result.erase(result.begin());
        while (!result.empty() && result.back() == '_')
            result.pop_back();

        return result.empty() ? fallback : result;
    }

    std::string CurrentMapName()
    {
        const char* levelName = gEngfuncs.pfnGetLevelName != nullptr ? gEngfuncs.pfnGetLevelName() : nullptr;
        std::string map = levelName != nullptr ? levelName : "";
        const size_t slash = map.find_last_of("/\\");
        if (slash != std::string::npos)
            map.erase(0, slash + 1);

        map = TrimExtension(map, ".bsp");
        return SanitizeDemoPart(map.c_str(), "map");
    }

    std::string CurrentPlayerName()
    {
        cvar_t* name = gEngfuncs.pfnGetCvarPointer != nullptr ? gEngfuncs.pfnGetCvarPointer("name") : nullptr;
        return SanitizeDemoPart((name != nullptr && name->string != nullptr) ? name->string : nullptr, "player");
    }

    struct JalaliDate
    {
        int year;
        int month;
        int day;
    };

    JalaliDate GregorianToJalali(int gy, int gm, int gd)
    {
        static constexpr int gDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        static constexpr int jDaysInMonth[] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29};

        gy -= 1600;
        gm -= 1;
        gd -= 1;

        int gDayNo = 365 * gy + (gy + 3) / 4 - (gy + 99) / 100 + (gy + 399) / 400;
        for (int i = 0; i < gm; ++i)
            gDayNo += gDaysInMonth[i];
        if (gm > 1 && ((gy + 1600) % 4 == 0 && ((gy + 1600) % 100 != 0 || (gy + 1600) % 400 == 0)))
            ++gDayNo;
        gDayNo += gd;

        int jDayNo = gDayNo - 79;
        const int jNp = jDayNo / 12053;
        jDayNo %= 12053;

        int jy = 979 + 33 * jNp + 4 * (jDayNo / 1461);
        jDayNo %= 1461;

        if (jDayNo >= 366)
        {
            jy += (jDayNo - 1) / 365;
            jDayNo = (jDayNo - 1) % 365;
        }

        int jm = 0;
        for (; jm < 11 && jDayNo >= jDaysInMonth[jm]; ++jm)
            jDayNo -= jDaysInMonth[jm];

        return {jy, jm + 1, jDayNo + 1};
    }

    std::string BuildDemoFileName()
    {
        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_s(&localTime, &now);

        char stamp[32]{};
        const JalaliDate date = GregorianToJalali(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
        std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d__%02d-%02d",
            date.year, date.month, date.day, localTime.tm_hour, localTime.tm_min);

        std::string demoName = CurrentPlayerName() + "_" + CurrentMapName() + "_" + stamp;
        constexpr size_t kDemoNameLimit = 79;
        if (demoName.size() > kDemoNameLimit)
            demoName.resize(kDemoNameLimit);
        while (!demoName.empty() && demoName.back() == '_')
            demoName.pop_back();

        return demoName.empty() ? std::string("allclient_demo") : demoName;
    }

    void EnsureDemoDirectory()
    {
#ifdef _WIN32
        _mkdir("cstrike\\demos");
#else
        mkdir("cstrike/demos", 0755);
#endif
    }

    void RunPendingDemoAction()
    {
        const DemoMenuAction action = g_PendingDemoAction;
        g_PendingDemoAction = DemoMenuAction::None;

        if (action == DemoMenuAction::Start)
        {
            EnsureDemoDirectory();
            const std::string command = "record \"demos/" + BuildDemoFileName() + "\"\n";
            gEngfuncs.pfnClientCmd(command.c_str());
        }
        else if (action == DemoMenuAction::Stop)
        {
            gEngfuncs.pfnClientCmd("stop\n");
        }
    }

    static int HUD_Key_EventHandler(int down, int keynum, const char* pszCurrentBinding, HUD_Key_EventNext next)
    {
        if (!down)
            return next->Invoke(down, keynum, pszCurrentBinding);

        if (IsDemoMenuKey(keynum) || BindingEquals(pszCurrentBinding, "allclient_demo_menu"))
        {
            ToggleDemoMenu();
            return 0;
        }

        if (!g_DemoMenuVisible)
            return next->Invoke(down, keynum, pszCurrentBinding);

        if (BindingEquals(pszCurrentBinding, "slot1") || keynum == '1')
        {
            g_PendingDemoAction = DemoMenuAction::Start;
            HideDemoMenu();
            return 0;
        }

        if (BindingEquals(pszCurrentBinding, "slot2") || keynum == '2')
        {
            g_PendingDemoAction = DemoMenuAction::Stop;
            HideDemoMenu();
            return 0;
        }

        if (BindingEquals(pszCurrentBinding, "slot10") || keynum == '0' || keynum == 27)
        {
            HideDemoMenu();
            return 0;
        }

        // Allow all other keys (movement W/A/S/D, jump, crouch, mouse buttons, etc.)
        // to pass through seamlessly so the player can continue playing while menu is open.
        return next->Invoke(down, keynum, pszCurrentBinding);
    }
}

cvar_t* hud_draw;

nitroapi::EngineData* eng()
{
    return g_NitroApi->GetEngineData();
}

nitroapi::ClientData* client()
{
    return g_NitroApi->GetClientData();
}

static void ClearHudTxt()
{
    if (!gHUD)
        return;

    if (*gHUD->m_pSpriteList)
        g_NitroApi->GetEngineData()->Mem_Free(*gHUD->m_pSpriteList);

    *gHUD->m_pSpriteList = nullptr;
    *gHUD->m_iSpriteCountAllRes = 0;
}

static void HUD_InitPost()
{
    CreateInterfaceFn gameui_factory = Sys_GetFactory(
#ifdef _WIN32
        "cstrike\\cl_dlls\\gameui.dll"
#else
        "cl_dlls/gameui.so"  // in linux version we have only original gameui under valve folder
#endif
        );

    g_GameConsole = (IGameConsole*)(InitializeInterface(GAMECONSOLE_INTERFACE_VERSION_GS, &gameui_factory, 1));
    g_GameConsoleNext = (IGameConsoleNext*)(InitializeInterface(GAMECONSOLE_NEXT_INTERFACE_VERSION, &gameui_factory, 1));

    std::memcpy(&cl_funcs, g_NitroApi->GetEngineData()->cldll_func, sizeof(cl_funcs));
    std::memcpy(&gEngfuncs, g_NitroApi->GetEngineData()->cl_enginefunc, sizeof(gEngfuncs));
    std::memcpy(&g_engfuncs, g_NitroApi->GetEngineData()->enginefuncs, sizeof(g_engfuncs));
    gHUD = g_NitroApi->GetClientData()->gHUD;
    InstallDynamicLightGuard();
    gEngfuncs.pfnAddCommand("allclient_demo_menu", ShowDemoMenuCommand);

    // Apply safe defaults once. These remain ordinary archived cvars and can
    // still be changed later in-game or by the planned external launcher.
    cvar_t* input_defaults_version = gEngfuncs.pfnGetCvarPointer("cl_input_defaults_version");
    if (input_defaults_version == nullptr)
        input_defaults_version = gEngfuncs.pfnRegisterVariable("cl_input_defaults_version", "0", FCVAR_ARCHIVE);

    if (input_defaults_version != nullptr && input_defaults_version->value < 1.0f)
    {
        gEngfuncs.Cvar_Set("m_rawinput", "1");
        gEngfuncs.Cvar_Set("m_filter", "0");
        gEngfuncs.Cvar_Set("joystick", "0");
        gEngfuncs.Cvar_Set("cl_input_defaults_version", "1");
    }
    sv = g_NitroApi->GetEngineData()->server;
    sv_static = g_NitroApi->GetEngineData()->server_static;

    ViewInit();
    FovInit();
    InspectInit();
    CameraInit();
    g_GameHud->Init();

    hud_draw = g_engfuncs.pfnCVarGetPointer("hud_draw");

    InvertMouseInit();

    ColorChatInConsolePatch();
}

static int HUD_RedrawHandler(float flTime, int iIntermission, HUD_RedrawNext next)
{
    const bool console_visible = g_GameConsole && g_GameConsole->IsConsoleVisible();
    const bool overlay_visible = console_visible;
    const float hud_draw_value = hud_draw->value;

    // Suppress both HUD layers only underneath the console. Restore the cvar
    // immediately afterwards so user configuration remains untouched. The
    // scoreboard intentionally keeps the complete HUD visible.
    if (overlay_visible)
        hud_draw->value = 0.0f;

    const int result = next->Invoke(flTime, iIntermission);

    if (overlay_visible)
        hud_draw->value = hud_draw_value;

    if (hud_draw_value != 0.0f && !overlay_visible)
        g_GameHud->Draw(flTime);

    if (hud_draw_value != 0.0f && !overlay_visible && g_DemoMenuVisible)
        DrawDemoMenu();

    if (g_PendingDemoAction != DemoMenuAction::None)
        RunPendingDemoAction();

    return result;
}

static void HUD_ResetHandler(HUD_ResetNext next)
{
    ClearHudTxt();

    next->Invoke();

    g_GameHud->Reset();
    ResetInvertMouse();
}

static int HUD_VidInitHandler(HUD_VidInitNext next)
{
    ClearHudTxt();

    next->Invoke();

    ViewVidInit();
    g_GameHud->VidInit();

    return 1;
}

static void Hook_V_CalcRefdef(ref_params_s* pparams, V_CalcRefdefNext next)
{
    ViewCalcRefdef(pparams, next);
}

static void HUD_UpdateClientDataPost(client_data_t* cdata, float flTime, int result)
{
    std::memcpy(&g_LastClientData, cdata, sizeof(client_data_t));

    FovHUD_UpdateClientData(cdata, flTime, result);
    g_GameHud->Think(flTime);
}

static void HUD_PostRunCmdPost(struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed)
{
    std::memcpy(&g_LastPlayerState, to, sizeof(local_state_t));
}

static void HUD_GetStudioModelInterfacePost(int version, r_studio_interface_t **ppinterface, engine_studio_api_t *pstudio, int result)
{
    std::memcpy(&IEngineStudio, pstudio, sizeof(IEngineStudio));
    std::memcpy(&g_OriginalStudio, *ppinterface, sizeof(g_OriginalStudio));

    (*ppinterface)->StudioDrawModel = StudioDrawModel;
    (*ppinterface)->StudioDrawPlayer = StudioDrawPlayer;
}

static void HUD_PlayerMoveInitPost(playermove_t* ppmove)
{
    pmove = ppmove;
}

static int UserMsg_SetFOVHandler(const char* name, int size, void* data, UserMsg_SetFOVNext next)
{
    return FovMsgFunc_SetFOV(name, size, data, next);
}

static void UserMsg_CurWeaponPost(const char* name, int size, void* data, int result)
{
    BEGIN_READ(data, size);

    int state = READ_BYTE();
    int weaponId = READ_CHAR();

    if (weaponId < 1)
        g_CurrentWeaponId = 0;
    else if (state)
        g_CurrentWeaponId = weaponId;
}

static void UserMsg_InitHUDPost(const char* name, int size, void* data, int result)
{
    g_GameHud->InitHUDData();
    ResetInvertMouse();
}

static int UserMsg_TextMsgHandler(const char* name, int size, void* data, UserMsg_TextMsgNext next)
{
    static const std::string hiddenServerCmds[] = {
        "client_chat_open\n",
        "client_chat_team_open\n",
        "client_chat_close\n",
    };

    BEGIN_READ(data, size);

    const int destType = READ_BYTE();
    if (destType == 2)
    {
        std::string message = READ_STRING();
        if (message == "#Game_unknown_command")
        {
            std::string command = READ_STRING();
            if (std::ranges::contains(hiddenServerCmds, command))
            {
                return 1;
            }
        }
    }

    return next->Invoke(name, size, data);
}

static void CL_CreateMoveHandler(float frametime, usercmd_t* cmd, int active, CL_CreateMoveNext next)
{
    if (IsInputBackground())
    {
        // Let the original client advance its command clock, but prevent a
        // focused gameplay command from surviving while our window is inactive.
        next->Invoke(frametime, cmd, 0);

        if (cmd != nullptr)
        {
            cmd->forwardmove = 0.0f;
            cmd->sidemove = 0.0f;
            cmd->upmove = 0.0f;
            cmd->buttons = 0;
            cmd->impulse = 0;
        }
        return;
    }
    CL_CreateMove_InvertMousePre(frametime, cmd, active);

    next->Invoke(frametime, cmd, active);

    CL_CreateMove_InvertMousePost(frametime, cmd, active);
}

static void HUD_ProcessPlayerStateHandler(entity_state_s* dst, const entity_state_s* src, HUD_ProcessPlayerStateNext next)
{
    cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();

    if (localPlayer->index == dst->number)
    {
        g_iUser1 = src->iuser1;
        g_iUser2 = src->iuser2;
    }

    next->Invoke(dst, src);
}

static void HUD_TempEntUpdateHandler(
    double frametime,
    double client_time,
    double cl_gravity,
    TEMPENTITY** ppTempEntFree,
    TEMPENTITY** ppTempEntActive,
    int (*Callback_AddVisibleEntity)(cl_entity_t*),
    void (*Callback_TempEntPlaySound)(TEMPENTITY*, float),
    HUD_TempEntUpdateNext next)
{
    // Spark showers repeatedly simulate gravity/collision and emit secondary
    // particles. Keep the normal impact feedback, but bound pathological
    // accumulation from sustained fire or effect-heavy servers. The original
    // updater remains solely responsible for unlinking and recycling entries.
    constexpr int kMaxActiveSparkShowers = 4;
    constexpr double kMaxSparkShowerLifetime = 0.15;
    // Bound the guard itself: a malicious or broken effect stream must not make
    // the client walk an unbounded linked list before the original update.
    constexpr int kMaxTempEntitiesToInspect = 256;

    if (ppTempEntActive != nullptr)
    {
        int active_spark_showers = 0;
        int inspected = 0;

        for (TEMPENTITY* temp = *ppTempEntActive;
             temp != nullptr && inspected < kMaxTempEntitiesToInspect;
             temp = temp->next, ++inspected)
        {
            if ((temp->flags & FTENT_SPARKSHOWER) == 0)
                continue;

            ++active_spark_showers;
            if (active_spark_showers > kMaxActiveSparkShowers)
            {
                temp->flags &= ~(FTENT_SPARKSHOWER | FTENT_HITSOUND);
                temp->die = static_cast<float>(client_time - 0.001);
                continue;
            }

            const float latest_die = static_cast<float>(client_time + kMaxSparkShowerLifetime);
            if (temp->die > latest_die)
                temp->die = latest_die;
        }
    }

    next->Invoke(frametime, client_time, cl_gravity, ppTempEntFree, ppTempEntActive,
                 Callback_AddVisibleEntity, Callback_TempEntPlaySound);
}

class ClientMini : public ClientMiniInterface
{
public:
    void Init(nitroapi::NitroApiInterface* nitro_api) override
    {
        g_NitroApi = nitro_api;
        gHUD = nullptr;

        MathLib_Init();

        g_MouseCaptureKnown = false;
        g_MouseCaptured = false;

        nitroapi::ClientData* client_data = nitro_api->GetClientData();
        g_Unsub.emplace_back(client_data->HUD_VidInit |= HUD_VidInitHandler);
        g_Unsub.emplace_back(client_data->HUD_Reset |= HUD_ResetHandler);
        g_Unsub.emplace_back(client_data->HUD_Init += HUD_InitPost);
        g_Unsub.emplace_back(client_data->HUD_Redraw |= HUD_RedrawHandler);
        g_Unsub.emplace_back(client_data->HUD_Key_Event |= HUD_Key_EventHandler);
        g_Unsub.emplace_back(client_data->HUD_UpdateClientData += HUD_UpdateClientDataPost);
        g_Unsub.emplace_back(client_data->V_CalcRefdef |= Hook_V_CalcRefdef);
        g_Unsub.emplace_back(client_data->HUD_PostRunCmd += HUD_PostRunCmdPost);
        g_Unsub.emplace_back(client_data->HUD_GetStudioModelInterface += HUD_GetStudioModelInterfacePost);
        g_Unsub.emplace_back(client_data->HUD_PlayerMoveInit += HUD_PlayerMoveInitPost);
        g_Unsub.emplace_back(client_data->UserMsg_SetFOV |= UserMsg_SetFOVHandler);
        g_Unsub.emplace_back(client_data->HUD_ProcessPlayerState |= HUD_ProcessPlayerStateHandler);
        g_Unsub.emplace_back(client_data->HUD_TempEntUpdate |= HUD_TempEntUpdateHandler);
        g_Unsub.emplace_back(client_data->UserMsg_CurWeapon += UserMsg_CurWeaponPost);
        g_Unsub.emplace_back(client_data->UserMsg_InitHUD += UserMsg_InitHUDPost);
        g_Unsub.emplace_back(client_data->UserMsg_TextMsg |= UserMsg_TextMsgHandler);
        g_Unsub.emplace_back(client_data->CL_CreateMove |= CL_CreateMoveHandler);

        // A capture transition is also an input-state boundary. Clearing once
        // here prevents held buttons and pre-capture mouse motion leaking into
        // the first gameplay command without polling or altering mouse deltas.
        g_Unsub.emplace_back(client_data->IN_ActivateMouse |= [](const auto& next) {
            next->Invoke();
            if (!g_MouseCaptureKnown || !g_MouseCaptured)
            {
                g_MouseCaptureKnown = true;
                g_MouseCaptured = true;
                g_InputBackground = false;
                IN_ClearStates();
                ResetInvertMouse();
            }
        });
        g_Unsub.emplace_back(client_data->IN_DeactivateMouse |= [](const auto& next) {
            next->Invoke();
            if (!g_MouseCaptureKnown || g_MouseCaptured)
            {
                g_MouseCaptureKnown = true;
                g_MouseCaptured = false;
                g_InputBackground = true;
                IN_ClearStates();
                ResetInvertMouse();
            }
        });

        g_GameHud = std::make_unique<GameHud>(nitro_api);
        g_Unsub.emplace_back(client_data->HUD_Shutdown += [] { g_GameHud.reset(); });

        // Sys_Error exits the process without Host_Shutdown, so HUD_Shutdown never fires on that path.
        g_Unsub.emplace_back(eng()->Sys_Error |= [](const char* error, const auto& next) {
            g_GameHud.reset();
            next->Invoke(error);
        });
    }

    void Uninitialize() override
    {
        RemoveDynamicLightGuard();
        ResetInvertMouse();
        g_MouseCaptureKnown = false;
        g_MouseCaptured = false;

        g_InputBackground = false;

        for (auto& unsubscriber: g_Unsub) {
            unsubscriber->Unsubscribe();
        }
        g_Unsub.clear();

        g_GameHud.reset();
        g_NitroApi = nullptr;
        g_GameConsole = nullptr;
        g_GameConsoleNext = nullptr;
        Q_memset(&cl_funcs, 0, sizeof(cl_funcs));
        Q_memset(&gEngfuncs, 0, sizeof(gEngfuncs));
        Q_memset(&g_LastPlayerState, 0, sizeof(g_LastPlayerState));
        Q_memset(&g_LastClientData, 0, sizeof(g_LastClientData));
        gHUD = nullptr;
        Q_memset(&IEngineStudio, 0, sizeof(IEngineStudio));
        Q_memset(&g_OriginalStudio, 0, sizeof(g_OriginalStudio));
        pmove = nullptr;
    }

    void GetVersion(char* buffer, int size) override
    {
        if (buffer != nullptr)
            V_strncpy(buffer, CLIENT_MINI_INTERFACE_VERSION ", " __DATE__ " " __TIME__, size);
    }
};

EXPOSE_SINGLE_INTERFACE(ClientMini, ClientMiniInterface, CLIENT_MINI_INTERFACE_VERSION);
