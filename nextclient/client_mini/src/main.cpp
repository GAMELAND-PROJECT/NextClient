#include "main.h"
#include <cstring>
#include <ranges>
#include <next_client_mini/client_mini.h>
#include <parsemsg.h>

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
