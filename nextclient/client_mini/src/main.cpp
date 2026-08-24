#include "main.h"
#include <cstring>
#include <ranges>
#include <next_client_mini/client_mini.h>
#include <parsemsg.h>

#ifdef _WIN32
#include <Windows.h>
#endif

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

#ifdef _WIN32
namespace
{
    enum class InputFocusState
    {
        Unknown,
        Background,
        Foreground,
    };

    InputFocusState g_InputFocusState = InputFocusState::Unknown;
    ULONGLONG g_NextFocusPollMs = 0;

    bool IsProcessForeground()
    {
        const HWND foreground_window = GetForegroundWindow();
        if (foreground_window == nullptr)
            return false;

        DWORD foreground_process_id = 0;
        GetWindowThreadProcessId(foreground_window, &foreground_process_id);
        static const DWORD current_process_id = GetCurrentProcessId();
        return foreground_process_id == current_process_id;
    }

    void UpdateInputFocusState()
    {
        const InputFocusState current_state = IsProcessForeground()
            ? InputFocusState::Foreground
            : InputFocusState::Background;

        if (g_InputFocusState == InputFocusState::Unknown)
        {
            g_InputFocusState = current_state;
            return;
        }

        if (current_state == g_InputFocusState)
            return;

        g_InputFocusState = current_state;

        // Clear transitions once on both focus edges so held movement,
        // attack states and stale mouse deltas cannot survive Alt+Tab.
        IN_ClearStates();
        ResetInvertMouse();
    }

    void PollInputFocusState()
    {
        const ULONGLONG now_ms = GetTickCount64();
        if (now_ms < g_NextFocusPollMs)
            return;

        // Focus changes do not need frame-rate polling. Ten checks per second
        // keep Alt+Tab handling responsive without doing Win32 queries every frame.
        g_NextFocusPollMs = now_ms + 100;
        UpdateInputFocusState();
    }

    bool IsInputBackground()
    {
        return g_InputFocusState == InputFocusState::Background;
    }
}
#endif

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
#ifdef _WIN32
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
#endif

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
                IN_ClearStates();
                ResetInvertMouse();
            }
        });

#ifdef _WIN32
        g_InputFocusState = InputFocusState::Unknown;
        g_NextFocusPollMs = 0;
        g_Unsub.emplace_back(eng()->Host_FrameInternal += [](float) { PollInputFocusState(); });
#endif

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
        ResetInvertMouse();
        g_MouseCaptureKnown = false;
        g_MouseCaptured = false;

#ifdef _WIN32
        g_InputFocusState = InputFocusState::Unknown;
#endif

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
