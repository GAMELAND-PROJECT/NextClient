#include "main.h"
#ifdef _WIN32
#include <direct.h>
#endif
#include <cctype>
#include <cmath>
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
#include "turn_speed_patch.h"
#include "inspect.h"
#include "invert_mouse.h"
#include "recorder/GameVideoRecorder.h"

using nextclient::client_mini::GameVideoRecorder;

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
static void GetGameScreenResolution(int& outWidth, int& outHeight)
{
    outWidth = 1024;
    outHeight = 768;
    if (gEngfuncs.pfnGetScreenInfo != nullptr)
    {
        SCREENINFO scr{};
        scr.iSize = sizeof(scr);
        if (gEngfuncs.pfnGetScreenInfo(&scr))
        {
            if (scr.iWidth > 0 && scr.iHeight > 0)
            {
                outWidth = scr.iWidth;
                outHeight = scr.iHeight;
            }
        }
    }
}

    bool g_InputBackground = false;
    bool g_DemoMenuVisible = false;
    enum class DemoMenuAction
    {
        None,
        StartDemo,
        StopDemo,
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

    bool IsPlayerInMatch()
    {
        const char* levelName = (gEngfuncs.pfnGetLevelName != nullptr) ? gEngfuncs.pfnGetLevelName() : nullptr;
        return (levelName != nullptr && levelName[0] != '\0');
    }

    bool IsClientDemoRecording()
    {
        if (GameVideoRecorder::Instance().IsMatchDemoRecording())
            return true;

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
        if (GameVideoRecorder::Instance().IsMatchDemoRecording())
        {
            return GameVideoRecorder::Instance().GetCurrentDemoFileName();
        }

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

    int g_SelectedDemoIndex = -1;
    int g_DemoPage = 0;

    std::string g_HighlightToastText;
    bool g_HighlightToastSuccess = false;
    double g_HighlightToastTimer = 0.0;
    int g_seekFrameCounter = 0;

static std::string GetActiveDemoOrMapName()
    {
        std::string demoName = GameVideoRecorder::Instance().GetCurrentPlayingDemoName();
        if (demoName.empty() || demoName == "Demo")
        {
            if (gEngfuncs.pfnGetLevelName != nullptr)
            {
                const char* lvl = gEngfuncs.pfnGetLevelName();
                if (lvl != nullptr && *lvl != 0)
                {
                    std::string s(lvl);
                    const size_t slash = s.find_last_of("/\\");
                    if (slash != std::string::npos) s = s.substr(slash + 1);
                    const size_t dot = s.find_last_of('.');
                    if (dot != std::string::npos) s = s.substr(0, dot);
                    demoName = s;
                }
            }
        }
        if (demoName.empty()) demoName = "Highlight";
        return demoName;
    }

    void DrawHighlightMarkingWidget(int scrW)
    {
        const int w = 250;
        const int h = 44;
        const int x = scrW - w - 16;
        const int y = 14;

        DrawHudBox(x, y, w, h, 14, 18, 24, 235);

        DrawHudBox(x, y, w, 1, 230, 50, 50, 230);
        DrawHudBox(x, y + h - 1, w, 1, 230, 50, 50, 230);
        DrawHudBox(x, y, 1, h, 230, 50, 50, 230);
        DrawHudBox(x + w - 1, y, 1, h, 230, 50, 50, 230);

        const double curTime = gEngfuncs.GetClientTime();
        const float pulse = static_cast<float>(0.55 + 0.45 * std::sin(curTime * 7.0));

        // Red recording indicator dot
        DrawHudBox(x + 10, y + 9, 10, 10, static_cast<int>(255 * pulse), 30, 30, 255);
        DrawHudBox(x + 13, y + 12, 4, 4, 255, 255, 255, 230);

        const double curDemo = GameVideoRecorder::Instance().GetExactDemoTime();
        const float inTime = GameVideoRecorder::Instance().GetMarkInTime();
        const float dur = (curDemo > inTime) ? static_cast<float>(curDemo - inTime) : 0.0f;

        char badge[64]{};
        std::snprintf(badge, sizeof(badge), "MARKING HIGHLIGHT: %s", GameVideoRecorder::Instance().GetFormattedTime(dur).c_str());

        gEngfuncs.pfnDrawSetTextColor(1.0f, 0.95f, 0.95f);
        DrawHudString(x + 28, y + 7, badge);

        char sub[64]{};
        std::snprintf(sub, sizeof(sub), "[2] Mark Out & Preview");
        gEngfuncs.pfnDrawSetTextColor(0.95f, 0.82f, 0.20f);
        DrawHudString(x + 10, y + 26, sub);
    }

    void DrawHighlightRenderingModal(int scrW, int scrH)
    {
        const int w = 480;
        const int h = 120;
        const int x = (scrW - w) / 2;
        const int y = (scrH - h) / 2;

        DrawHudBox(x, y, w, h, 12, 16, 22, 245);

        DrawHudBox(x, y, w, 3, 0, 200, 255, 255);

        DrawHudBox(x, y, w, 1, 50, 75, 100, 180);
        DrawHudBox(x, y + h - 1, w, 1, 50, 75, 100, 180);
        DrawHudBox(x, y, 1, h, 50, 75, 100, 180);
        DrawHudBox(x + w - 1, y, 1, h, 50, 75, 100, 180);

        gEngfuncs.pfnDrawSetTextColor(0.20f, 0.85f, 1.0f);
        DrawHudString(x + 20, y + 14, "GAMELAND DEMO STUDIO | RENDERING 1080P MP4");

        const bool isSeeking = GameVideoRecorder::Instance().IsHighlightSeeking();
        const int pct = isSeeking ? 0 : GameVideoRecorder::Instance().GetRenderProgressPercent();
        const uint64_t pushed = isSeeking ? 0 : GameVideoRecorder::Instance().GetRenderFramesPushed();
        const uint64_t total = GameVideoRecorder::Instance().GetRenderTargetFrames();

        const int barX = x + 20;
        const int barY = y + 42;
        const int barW = w - 40;
        const int barH = 16;

        DrawHudBox(barX, barY, barW, barH, 20, 28, 38, 255);
        const int fillW = (pct * barW) / 100;
        if (fillW > 0)
            DrawHudBox(barX, barY, fillW, barH, 0, 210, 255, 255);

        DrawHudBox(barX, barY, barW, 1, 60, 100, 140, 220);
        DrawHudBox(barX, barY + barH - 1, barW, 1, 60, 100, 140, 220);
        DrawHudBox(barX, barY, 1, barH, 60, 100, 140, 220);
        DrawHudBox(barX + barW - 1, barY, 1, barH, 60, 100, 140, 220);

        char progText[96]{};
        if (isSeeking)
        {
            std::snprintf(progText, sizeof(progText), "Jumping to Highlight Start... Seeking packets");
        }
        else
        {
            std::snprintf(progText, sizeof(progText), "Progress: %d%%  |  Frame %llu of %llu  |  60 FPS Lockstep", pct, pushed, total);
        }
        gEngfuncs.pfnDrawSetTextColor(0.95f, 0.95f, 0.95f);
        DrawHudString(x + 20, y + 68, progText);

        gEngfuncs.pfnDrawSetTextColor(0.60f, 0.75f, 0.85f);
        DrawHudString(x + 20, y + 92, isSeeking ? "Preparing 1080p studio lockstep capture..." : "Rendering in background... (~2-3 sec)");
    }

    void DrawHighlightConfirmDialog(int scrW, int scrH)
    {
        const int w = 510;
        const int h = 185;
        const int x = (scrW - w) / 2;
        const int y = (scrH - h) / 2;

        DrawHudBox(x, y, w, h, 12, 16, 22, 245);

        DrawHudBox(x, y, w, 3, 75, 210, 255, 255);

        DrawHudBox(x, y, w, 1, 60, 85, 110, 180);
        DrawHudBox(x, y + h - 1, w, 1, 60, 85, 110, 180);
        DrawHudBox(x, y, 1, h, 60, 85, 110, 180);
        DrawHudBox(x + w - 1, y, 1, h, 60, 85, 110, 180);

        gEngfuncs.pfnDrawSetTextColor(0.30f, 0.85f, 1.0f);
        DrawHudString(x + 20, y + 14, "EXPORT HIGHLIGHT TO MP4?  |  DEMO STUDIO");

        DrawHudBox(x + 16, y + 36, w - 32, 1, 60, 80, 100, 140);

        const std::string clipInfo = GameVideoRecorder::Instance().GetHighlightClipInfo();
        gEngfuncs.pfnDrawSetTextColor(0.95f, 0.95f, 0.95f);
        DrawHudString(x + 20, y + 48, clipInfo.c_str());

        gEngfuncs.pfnDrawSetTextColor(0.65f, 0.78f, 0.90f);
        DrawHudString(x + 20, y + 70, "Quality: 1080p Full HD (Lanczos) | 60 FPS Locked | HLAE Lockstep");

        DrawHudBox(x + 16, y + 94, w - 32, 1, 50, 70, 90, 120);

        gEngfuncs.pfnDrawSetTextColor(0.20f, 1.0f, 0.40f);
        DrawHudString(x + 20, y + 110, "[ 1 ]  YES, EXPORT TO VIDEOS/ (Press 1 or Enter)");

        gEngfuncs.pfnDrawSetTextColor(1.0f, 0.35f, 0.35f);
        DrawHudString(x + 20, y + 138, "[ 2 ]  NO, DISCARD AND CANCEL (Press 2 or Esc)");
    }

    void DrawHighlightToast(int scrW, int scrH)
    {
        const int w = 510;
        const int h = 32;
        const int x = (scrW - w) / 2;
        const int y = 20;

        DrawHudBox(x, y, w, h, 14, 18, 24, 235);
        if (g_HighlightToastSuccess)
        {
            DrawHudBox(x, y, w, 2, 40, 220, 80, 255);
            gEngfuncs.pfnDrawSetTextColor(0.25f, 1.0f, 0.40f);
        }
        else
        {
            DrawHudBox(x, y, w, 2, 240, 60, 60, 255);
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.40f, 0.40f);
        }
        DrawHudString(x + 16, y + 9, g_HighlightToastText.c_str());
    }

    void DrawHighlightIdleHint(int scrW)
    {
        const int w = 175;
        const int h = 24;
        const int x = scrW - w - 16;
        const int y = 14;

        DrawHudBox(x, y, w, h, 14, 18, 22, 180);
        DrawHudBox(x, y, w, 1, 60, 75, 90, 120);
        DrawHudBox(x, y + h - 1, w, 1, 60, 75, 90, 120);
        DrawHudBox(x, y, 1, h, 60, 75, 90, 120);
        DrawHudBox(x + w - 1, y, 1, h, 60, 75, 90, 120);

        gEngfuncs.pfnDrawSetTextColor(0.95f, 0.82f, 0.20f);
        DrawHudString(x + 12, y + 6, "[1] Mark Highlight Start");
    }

    void DrawWindowsCaptureWidget(int scrW)
    {
        const int w = 135;
        const int h = 26;
        const int x = scrW - w - 16;
        const int y = 14;

        // Dark modern translucent glass pill background (Windows Capture Style)
        DrawHudBox(x, y, w, h, 14, 18, 22, 225);

        // Modern 1px border
        DrawHudBox(x, y, w, 1, 55, 70, 85, 190);
        DrawHudBox(x, y + h - 1, w, 1, 55, 70, 85, 190);
        DrawHudBox(x, y, 1, h, 55, 70, 85, 190);
        DrawHudBox(x + w - 1, y, 1, h, 55, 70, 85, 190);

        // Pulsing red recording dot
        const double curTime = gEngfuncs.GetClientTime();
        const float pulse = static_cast<float>(0.65 + 0.35 * std::sin(curTime * 5.0));

        // Red outer dot
        DrawHudBox(x + 10, y + 8, 10, 10, static_cast<int>(255 * pulse), 25, 25, 255);
        // Center white dot for glowing glass effect
        DrawHudBox(x + 13, y + 11, 4, 4, 255, 255, 255, 220);

        // Windows capture style text: "REC 01:23"
        const std::string timeStr = GameVideoRecorder::Instance().GetFormattedDemoTime();
        char badge[48]{};
        std::snprintf(badge, sizeof(badge), "REC  %s", timeStr.c_str());

        gEngfuncs.pfnDrawSetTextColor(0.95f, 0.95f, 0.95f);
        DrawHudString(x + 28, y + 7, badge);
    }

    void DrawMatchDemoMenu()
    {
        const bool isRecording = IsClientDemoRecording();

        const int menuX = 35;
        const int menuY = 130;
        const int menuW = 295;
        const int menuH = 175;

        // Background panel (Dark translucent CS-style)
        DrawHudBox(menuX, menuY, menuW, menuH, 12, 16, 20, 220);

        // Header accent bar (Gold / Amber)
        DrawHudBox(menuX, menuY, menuW, 3, 255, 178, 28, 255);

        // Border outline
        DrawHudBox(menuX, menuY, menuW, 1, 60, 75, 90, 160);
        DrawHudBox(menuX, menuY + menuH - 1, menuW, 1, 60, 75, 90, 160);
        DrawHudBox(menuX, menuY, 1, menuH, 60, 75, 90, 160);
        DrawHudBox(menuX + menuW - 1, menuY, 1, menuH, 60, 75, 90, 160);

        // Header Title
        gEngfuncs.pfnDrawSetTextColor(1.0f, 0.78f, 0.12f);
        DrawHudString(menuX + 16, menuY + 12, "GAMELAND MATCH RECORDER");

        // Separator line
        DrawHudBox(menuX + 12, menuY + 34, menuW - 24, 1, 70, 85, 100, 120);

        // Status section
        gEngfuncs.pfnDrawSetTextColor(0.70f, 0.75f, 0.80f);
        DrawHudString(menuX + 16, menuY + 44, "STATUS:");

        if (isRecording)
        {
            // Vivid green pulse indicator
            gEngfuncs.pfnDrawSetTextColor(0.15f, 1.0f, 0.25f);
            char recStatus[64]{};
            const std::string timeStr = GameVideoRecorder::Instance().GetFormattedDemoTime();
            std::snprintf(recStatus, sizeof(recStatus), "[*] RECORDING [%s]", timeStr.c_str());
            DrawHudString(menuX + 75, menuY + 44, recStatus);

            const std::string curDemo = GetCurrentRecordingDemoName();
            char demoInfo[96]{};
            std::snprintf(demoInfo, sizeof(demoInfo), "File: %s", curDemo.c_str());
            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.80f, 0.60f);
            DrawHudString(menuX + 16, menuY + 62, demoInfo);
        }
        else
        {
            // Standby
            gEngfuncs.pfnDrawSetTextColor(0.95f, 0.40f, 0.20f);
            DrawHudString(menuX + 75, menuY + 44, "[o] STANDBY / IDLE");

            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.75f, 0.85f);
            DrawHudString(menuX + 16, menuY + 62, "Zero-Lag Match Mode (100 FPS Locked)");
        }

        // Sub separator
        DrawHudBox(menuX + 12, menuY + 84, menuW - 24, 1, 55, 65, 75, 100);

        // Menu Option 1
        if (isRecording)
        {
            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.55f, 0.55f);
            DrawHudString(menuX + 16, menuY + 95, "1. Start Recording (Active)");
        }
        else
        {
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.82f, 0.20f);
            DrawHudString(menuX + 16, menuY + 95, "1. Start Match Demo Recording");
        }

        // Menu Option 2
        if (isRecording)
        {
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.30f, 0.30f);
            DrawHudString(menuX + 16, menuY + 118, "2. Stop Recording & Save Demo");
        }
        else
        {
            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.55f, 0.55f);
            DrawHudString(menuX + 16, menuY + 118, "2. Stop Recording (Inactive)");
        }

        // Bottom separator
        DrawHudBox(menuX + 12, menuY + 142, menuW - 24, 1, 55, 65, 75, 100);

        // Menu Option 0 / F4 Close
        gEngfuncs.pfnDrawSetTextColor(0.75f, 0.78f, 0.82f);
        DrawHudString(menuX + 16, menuY + 151, "0. Close Menu  (Press F4)");
    }

    void DrawLobbyDemoStudio()
    {
        const int menuX = 35;
        const int menuY = 100;
        const int menuW = 340;
        const int menuH = 260;

        // Background panel (Dark translucent high-tech CS-style)
        DrawHudBox(menuX, menuY, menuW, menuH, 10, 14, 20, 230);

        // Header accent bar (Cyan / Electric Blue)
        DrawHudBox(menuX, menuY, menuW, 3, 0, 180, 255, 255);

        // Border outline
        DrawHudBox(menuX, menuY, menuW, 1, 40, 80, 120, 180);
        DrawHudBox(menuX, menuY + menuH - 1, menuW, 1, 40, 80, 120, 180);
        DrawHudBox(menuX, menuY, 1, menuH, 40, 80, 120, 180);
        DrawHudBox(menuX + menuW - 1, menuY, 1, menuH, 40, 80, 120, 180);

        // Header Title
        gEngfuncs.pfnDrawSetTextColor(0.0f, 0.85f, 1.0f);
        DrawHudString(menuX + 16, menuY + 12, "GAMELAND MATCH DEMO STUDIO");

        // Separator line
        DrawHudBox(menuX + 12, menuY + 32, menuW - 24, 1, 40, 75, 100, 130);

        const bool isConverting = GameVideoRecorder::Instance().IsConverting();

        if (isConverting)
        {
            // Status: Converting
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.78f, 0.15f);
            DrawHudString(menuX + 16, menuY + 44, "CONVERTING TO MP4 (BACKGROUND)");

            const std::string curDemo = GameVideoRecorder::Instance().GetConvertingDemoName();
            char demoInfo[96]{};
            std::snprintf(demoInfo, sizeof(demoInfo), "Demo: %s", curDemo.c_str());
            gEngfuncs.pfnDrawSetTextColor(0.80f, 0.85f, 0.90f);
            DrawHudString(menuX + 16, menuY + 64, demoInfo);

            // Progress bar
            const int percent = GameVideoRecorder::Instance().GetConversionPercent();
            const int barW = menuW - 32;
            const int barH = 14;
            const int barX = menuX + 16;
            const int barY = menuY + 90;

            // Bar background
            DrawHudBox(barX, barY, barW, barH, 20, 30, 40, 255);
            // Bar fill
            const int fillW = (percent * barW) / 100;
            if (fillW > 0)
                DrawHudBox(barX, barY, fillW, barH, 0, 190, 240, 255);
            // Bar border
            DrawHudBox(barX, barY, barW, 1, 60, 120, 180, 200);
            DrawHudBox(barX, barY + barH - 1, barW, 1, 60, 120, 180, 200);
            DrawHudBox(barX, barY, 1, barH, 60, 120, 180, 200);
            DrawHudBox(barX + barW - 1, barY, 1, barH, 60, 120, 180, 200);

            char progText[48]{};
            std::snprintf(progText, sizeof(progText), "Conversion Progress: %d%%", percent);
            gEngfuncs.pfnDrawSetTextColor(0.95f, 0.95f, 0.95f);
            DrawHudString(menuX + 16, menuY + 112, progText);

            gEngfuncs.pfnDrawSetTextColor(0.60f, 0.70f, 0.80f);
            DrawHudString(menuX + 16, menuY + 134, "Silent rendering at 1080p 60 FPS...");

            DrawHudBox(menuX + 12, menuY + 165, menuW - 24, 1, 40, 75, 100, 130);

            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.35f, 0.35f);
            DrawHudString(menuX + 16, menuY + 178, "1. Cancel Conversion");

            gEngfuncs.pfnDrawSetTextColor(0.70f, 0.85f, 1.0f);
            DrawHudString(menuX + 16, menuY + 202, "9. Open Videos Folder");

            gEngfuncs.pfnDrawSetTextColor(0.75f, 0.78f, 0.82f);
            DrawHudString(menuX + 16, menuY + 226, "0. Close Studio  (Press F4)");
            return;
        }

        const auto& demos = GameVideoRecorder::Instance().GetCachedDemos();

        if (g_SelectedDemoIndex >= 0 && g_SelectedDemoIndex < static_cast<int>(demos.size()))
        {
            // Confirmation Screen
            const auto& sel = demos[g_SelectedDemoIndex];
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.85f, 0.20f);
            DrawHudString(menuX + 16, menuY + 44, "CONFIRM CONVERSION TO MP4:");

            char selInfo[96]{};
            std::snprintf(selInfo, sizeof(selInfo), "Demo: %s", sel.fileName.c_str());
            gEngfuncs.pfnDrawSetTextColor(0.90f, 0.90f, 0.90f);
            DrawHudString(menuX + 16, menuY + 66, selInfo);

            char metaInfo[96]{};
            std::snprintf(metaInfo, sizeof(metaInfo), "Map: %s | Size: %s", sel.mapName.c_str(), sel.sizeFormatted.c_str());
            gEngfuncs.pfnDrawSetTextColor(0.65f, 0.80f, 0.85f);
            DrawHudString(menuX + 16, menuY + 86, metaInfo);

            gEngfuncs.pfnDrawSetTextColor(0.55f, 0.85f, 0.65f);
            DrawHudString(menuX + 16, menuY + 110, "Target: 1080p 60 FPS Crystal Clear MP4");
            gEngfuncs.pfnDrawSetTextColor(0.60f, 0.65f, 0.75f);
            DrawHudString(menuX + 16, menuY + 128, "Runs silently in background with 0% lag");

            DrawHudBox(menuX + 12, menuY + 152, menuW - 24, 1, 40, 75, 100, 130);

            gEngfuncs.pfnDrawSetTextColor(0.20f, 1.0f, 0.35f);
            DrawHudString(menuX + 16, menuY + 166, "1. CONFIRM & START CONVERTING");

            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.40f, 0.40f);
            DrawHudString(menuX + 16, menuY + 190, "2. Cancel Selection / Go Back");

            gEngfuncs.pfnDrawSetTextColor(0.75f, 0.78f, 0.82f);
            DrawHudString(menuX + 16, menuY + 226, "0. Close Studio  (Press F4)");
            return;
        }

        // Normal Demo Browser
        if (demos.empty())
        {
            gEngfuncs.pfnDrawSetTextColor(0.85f, 0.60f, 0.20f);
            DrawHudString(menuX + 16, menuY + 50, "No demos found in cstrike/demos/");

            gEngfuncs.pfnDrawSetTextColor(0.65f, 0.70f, 0.75f);
            DrawHudString(menuX + 16, menuY + 75, "Play a match and press F4 to record!");

            DrawHudBox(menuX + 12, menuY + 165, menuW - 24, 1, 40, 75, 100, 130);

            gEngfuncs.pfnDrawSetTextColor(0.70f, 0.85f, 1.0f);
            DrawHudString(menuX + 16, menuY + 195, "9. Open Videos Folder");

            gEngfuncs.pfnDrawSetTextColor(0.75f, 0.78f, 0.82f);
            DrawHudString(menuX + 16, menuY + 226, "0. Close Studio  (Press F4)");
            return;
        }

        gEngfuncs.pfnDrawSetTextColor(0.80f, 0.85f, 0.90f);
        DrawHudString(menuX + 16, menuY + 42, "SELECT A DEMO TO CONVERT TO MP4:");

        int startIdx = g_DemoPage * 4;
        for (int i = 0; i < 4; ++i)
        {
            int demoIdx = startIdx + i;
            int itemY = menuY + 64 + (i * 26);
            if (demoIdx < static_cast<int>(demos.size()))
            {
                const auto& d = demos[demoIdx];
                char line[96]{};
                std::snprintf(line, sizeof(line), "%d. [%s] %s (%s)", i + 1, d.mapName.c_str(), d.dateFormatted.c_str(), d.sizeFormatted.c_str());
                gEngfuncs.pfnDrawSetTextColor(1.0f, 0.85f, 0.25f);
                DrawHudString(menuX + 16, itemY, line);
            }
            else
            {
                gEngfuncs.pfnDrawSetTextColor(0.35f, 0.40f, 0.45f);
                char line[32]{};
                std::snprintf(line, sizeof(line), "%d. (Empty slot)", i + 1);
                DrawHudString(menuX + 16, itemY, line);
            }
        }

        DrawHudBox(menuX + 12, menuY + 172, menuW - 24, 1, 40, 75, 100, 130);

        // Footer / Controls
        gEngfuncs.pfnDrawSetTextColor(0.70f, 0.85f, 1.0f);
        DrawHudString(menuX + 16, menuY + 184, "9. Open Videos Folder");

        // Next page if more than 4 demos
        if (demos.size() > 4)
        {
            gEngfuncs.pfnDrawSetTextColor(0.85f, 0.75f, 0.35f);
            DrawHudString(menuX + 180, menuY + 184, "8. Next Page ->");
        }

        gEngfuncs.pfnDrawSetTextColor(0.75f, 0.78f, 0.82f);
        DrawHudString(menuX + 16, menuY + 226, "0. Close Studio  (Press F4)");
    }

    void DrawDemoMenu()
    {
        if (IsPlayerInMatch())
            DrawMatchDemoMenu();
        else
            DrawLobbyDemoStudio();
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
        _mkdir("demos");
#else
        mkdir("demos", 0755);
#endif
    }

    void RunPendingDemoAction()
    {
        const DemoMenuAction action = g_PendingDemoAction;
        g_PendingDemoAction = DemoMenuAction::None;

        if (action == DemoMenuAction::StartDemo)
        {
            EnsureDemoDirectory();
            const std::string baseName = BuildDemoFileName();

            // 1. Start GoldSrc engine demo (0% GPU flush, locked 100 FPS)
            const std::string command = "record \"demos/" + baseName + "\"\n";
            gEngfuncs.pfnClientCmd(command.c_str());

            // 2. Track demo start time for top-right Windows capture widget
            GameVideoRecorder::Instance().StartMatchDemo(baseName);

            char msg[128]{};
            std::snprintf(msg, sizeof(msg), "\n^2[Gameland Match Recorder] Started match demo: demos/%s.dem\n\n", baseName.c_str());
            gEngfuncs.pfnConsolePrint(msg);
        }
        else if (action == DemoMenuAction::StopDemo)
        {
            // Stop GoldSrc demo
            gEngfuncs.pfnClientCmd("stop\n");
            GameVideoRecorder::Instance().StopMatchDemo();

            gEngfuncs.pfnConsolePrint("\n^2[Gameland Match Recorder] Match demo stopped and saved to demos/!\n\n");
        }
    }

    static int HUD_Key_EventHandler(int down, int keynum, const char* pszCurrentBinding, HUD_Key_EventNext next)
    {
        if (!down)
            return next->Invoke(down, keynum, pszCurrentBinding);

        const bool isViewingDemo = (gEngfuncs.pDemoAPI && gEngfuncs.pDemoAPI->IsPlayingback());
        if (isViewingDemo)
        {
            // Block inputs while seeking or rendering lockstep frames
            if (GameVideoRecorder::Instance().IsHighlightSeeking() || GameVideoRecorder::Instance().IsHighlightRendering())
            {
                return 0;
            }

            if (GameVideoRecorder::Instance().IsHighlightAwaitingConfirm())
            {
                if (BindingEquals(pszCurrentBinding, "slot1") || keynum == '1' || keynum == 13) // 1 or Enter
                {
                    // Direct absolute jump to mark in timestamp (NO relative dem_jump offset!)
                    const float markIn = GameVideoRecorder::Instance().GetMarkInTime();
                    GameVideoRecorder::Instance().SetDemoWorldTime(static_cast<double>(markIn), false);
                    GameVideoRecorder::Instance().SetDemoPaused(true);
                    gEngfuncs.pfnClientCmd("dem_speed 1.0\n");

                    GameVideoRecorder::Instance().SetHighlightSeeking();
                    g_seekFrameCounter = 0;
                    return 0;
                }

                if (BindingEquals(pszCurrentBinding, "slot2") || keynum == '2' || keynum == 27 || keynum == '0') // 2 or Esc
                {
                    GameVideoRecorder::Instance().DiscardHighlight();
                    GameVideoRecorder::Instance().SetDemoPaused(false);
                    gEngfuncs.pfnClientCmd("dem_pause 0\n"); // resume playback
                    g_HighlightToastText = "Highlight discarded";
                    g_HighlightToastSuccess = false;
                    g_HighlightToastTimer = gEngfuncs.GetClientTime() + 3.0;
                    return 0;
                }

                return 0;
            }

            if (GameVideoRecorder::Instance().IsHighlightMarking())
            {
                if (BindingEquals(pszCurrentBinding, "slot2") || keynum == '2') // 2 = Mark Out
                {
                    const double curDemo = GameVideoRecorder::Instance().GetExactDemoTime();
                    if (GameVideoRecorder::Instance().MarkOut(static_cast<float>(curDemo)))
                    {
                        GameVideoRecorder::Instance().SetDemoPaused(true);
                        gEngfuncs.pfnClientCmd("dem_pause 1\n"); // pause on mark out
                    }
                    else
                    {
                        g_HighlightToastText = "Highlight too short (< 0.5s)";
                        g_HighlightToastSuccess = false;
                        g_HighlightToastTimer = gEngfuncs.GetClientTime() + 3.0;
                    }
                    return 0;
                }

                if (keynum == 27 || keynum == '0') // Cancel
                {
                    GameVideoRecorder::Instance().DiscardHighlight();
                    GameVideoRecorder::Instance().SetDemoPaused(false);
                    gEngfuncs.pfnClientCmd("dem_pause 0\n");
                    g_HighlightToastText = "Highlight cancelled";
                    g_HighlightToastSuccess = false;
                    g_HighlightToastTimer = gEngfuncs.GetClientTime() + 2.0;
                    return 0;
                }
            }
            else // HighlightState::Idle
            {
                if (BindingEquals(pszCurrentBinding, "slot1") || keynum == '1') // 1 = Mark In
                {
                    const double curDemo = GameVideoRecorder::Instance().GetExactDemoTime();
                    GameVideoRecorder::Instance().MarkIn(static_cast<float>(curDemo));
                    GameVideoRecorder::Instance().SetDemoPaused(false);
                    gEngfuncs.pfnClientCmd("dem_speed 1.0\n");
                    gEngfuncs.pfnClientCmd("dem_pause 0\n");
                    return 0;
                }
            }
        }

        if (IsDemoMenuKey(keynum) || BindingEquals(pszCurrentBinding, "allclient_demo_menu"))
        {
            if (!g_DemoMenuVisible)
            {
                g_DemoMenuVisible = true;
                g_SelectedDemoIndex = -1;
                if (!IsPlayerInMatch())
                    GameVideoRecorder::Instance().RefreshDemoList();
            }
            else
            {
                g_DemoMenuVisible = false;
            }
            return 0;
        }

        if (!g_DemoMenuVisible)
            return next->Invoke(down, keynum, pszCurrentBinding);

        // In-Match Key Handling
        if (IsPlayerInMatch())
        {
            if (BindingEquals(pszCurrentBinding, "slot1") || keynum == '1')
            {
                g_PendingDemoAction = DemoMenuAction::StartDemo;
                HideDemoMenu();
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot2") || keynum == '2')
            {
                g_PendingDemoAction = DemoMenuAction::StopDemo;
                HideDemoMenu();
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot10") || keynum == '0' || keynum == 27)
            {
                HideDemoMenu();
                return 0;
            }

            return next->Invoke(down, keynum, pszCurrentBinding);
        }

        // In-Lobby Studio Key Handling
        if (GameVideoRecorder::Instance().IsConverting())
        {
            if (BindingEquals(pszCurrentBinding, "slot1") || keynum == '1')
            {
                GameVideoRecorder::Instance().CancelConversion();
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot9") || keynum == '9')
            {
                WinExec("explorer.exe cstrike\\videos", SW_SHOW);
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot10") || keynum == '0' || keynum == 27)
            {
                HideDemoMenu();
                return 0;
            }

            return 0;
        }

        // Confirming Conversion Screen
        const auto& demos = GameVideoRecorder::Instance().GetCachedDemos();
        if (g_SelectedDemoIndex >= 0 && g_SelectedDemoIndex < static_cast<int>(demos.size()))
        {
            if (BindingEquals(pszCurrentBinding, "slot1") || keynum == '1' || keynum == 13) // Enter or 1
            {
                const std::string demoFile = demos[g_SelectedDemoIndex].fileName;
                int curW = 1024, curH = 768;
                GetGameScreenResolution(curW, curH);
                GameVideoRecorder::Instance().StartDemoConversion(demoFile, curW, curH, 100);
                g_SelectedDemoIndex = -1;
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot2") || keynum == '2' || keynum == 27)
            {
                g_SelectedDemoIndex = -1;
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot10") || keynum == '0')
            {
                g_SelectedDemoIndex = -1;
                HideDemoMenu();
                return 0;
            }

            return 0;
        }

        // Normal Demo List Browsing
        if (demos.empty())
        {
            if (BindingEquals(pszCurrentBinding, "slot9") || keynum == '9')
            {
                WinExec("explorer.exe cstrike\\videos", SW_SHOW);
                return 0;
            }

            if (BindingEquals(pszCurrentBinding, "slot10") || keynum == '0' || keynum == 27)
            {
                HideDemoMenu();
                return 0;
            }
            return 0;
        }

        // Slot 1..4 selects demo
        if ((BindingEquals(pszCurrentBinding, "slot1") || keynum == '1') && demos.size() > 0)
        {
            int idx = g_DemoPage * 4 + 0;
            if (idx < static_cast<int>(demos.size()))
                g_SelectedDemoIndex = idx;
            return 0;
        }
        if ((BindingEquals(pszCurrentBinding, "slot2") || keynum == '2') && demos.size() > 1)
        {
            int idx = g_DemoPage * 4 + 1;
            if (idx < static_cast<int>(demos.size()))
                g_SelectedDemoIndex = idx;
            return 0;
        }
        if ((BindingEquals(pszCurrentBinding, "slot3") || keynum == '3') && demos.size() > 2)
        {
            int idx = g_DemoPage * 4 + 2;
            if (idx < static_cast<int>(demos.size()))
                g_SelectedDemoIndex = idx;
            return 0;
        }
        if ((BindingEquals(pszCurrentBinding, "slot4") || keynum == '4') && demos.size() > 3)
        {
            int idx = g_DemoPage * 4 + 3;
            if (idx < static_cast<int>(demos.size()))
                g_SelectedDemoIndex = idx;
            return 0;
        }

        // Next page
        if ((BindingEquals(pszCurrentBinding, "slot8") || keynum == '8') && demos.size() > 4)
        {
            int maxPages = (static_cast<int>(demos.size()) + 3) / 4;
            g_DemoPage = (g_DemoPage + 1) % maxPages;
            return 0;
        }

        // Open videos folder
        if (BindingEquals(pszCurrentBinding, "slot9") || keynum == '9')
        {
            WinExec("explorer.exe cstrike\\videos", SW_SHOW);
            return 0;
        }

        if (BindingEquals(pszCurrentBinding, "slot10") || keynum == '0' || keynum == 27)
        {
            HideDemoMenu();
            return 0;
        }

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
    TurnSpeedLimitPatch();
}

static void FixDemoScoreboardTeams()
{
    if (g_NitroApi == nullptr || g_NitroApi->GetClientData() == nullptr || g_NitroApi->GetClientData()->g_PlayerExtraInfo == nullptr)
        return;

    extra_player_info_t* extraInfo = g_NitroApi->GetClientData()->g_PlayerExtraInfo;

    for (int i = 1; i <= 32; ++i)
    {
        hud_player_info_t info{};
        gEngfuncs.pfnGetPlayerInfo(i, &info);
        if (info.name == nullptr || info.name[0] == '\0')
            continue;

        extra_player_info_t& extra = extraInfo[i];
        if (extra.teamname[0] == '\0' || (std::strcmp(extra.teamname, "TERRORIST") != 0 && std::strcmp(extra.teamname, "CT") != 0))
        {
            if (info.model != nullptr && info.model[0] != '\0')
            {
                std::string model = info.model;
                for (char& c : model) c = static_cast<char>(tolower(c));

                if (model.find("terror") != std::string::npos ||
                    model.find("leet") != std::string::npos ||
                    model.find("arctic") != std::string::npos ||
                    model.find("guerilla") != std::string::npos)
                {
                    extra.teamnumber = 1; // TEAM_TERRORIST
                    strcpy_s(extra.teamname, sizeof(extra.teamname), "TERRORIST");
                }
                else if (model.find("urban") != std::string::npos ||
                         model.find("gsg9") != std::string::npos ||
                         model.find("sas") != std::string::npos ||
                         model.find("gign") != std::string::npos ||
                         model.find("vip") != std::string::npos)
                {
                    extra.teamnumber = 2; // TEAM_CT
                    strcpy_s(extra.teamname, sizeof(extra.teamname), "CT");
                }
            }
        }
    }
}

static int HUD_RedrawHandler(float flTime, int iIntermission, HUD_RedrawNext next)
{
    const bool console_visible = g_GameConsole && g_GameConsole->IsConsoleVisible();
    const bool overlay_visible = console_visible;
    const float hud_draw_value = hud_draw->value;

    if (overlay_visible)
        hud_draw->value = 0.0f;

    const int result = next->Invoke(flTime, iIntermission);

    if (overlay_visible)
        hud_draw->value = hud_draw_value;

    if (hud_draw_value != 0.0f && !overlay_visible)
        g_GameHud->Draw(flTime);

    int scrW = 1024, scrH = 768;
    GetGameScreenResolution(scrW, scrH);

    // HLAE Studio Lockstep Seek & Capture (Exact Absolute Jump)
    if (GameVideoRecorder::Instance().IsHighlightSeeking())
    {
        g_seekFrameCounter++;

        // Allow 6 frames for engine to decompress delta packets and reconstruct entities at markIn
        if (g_seekFrameCounter >= 6)
        {
            const std::string name = GetActiveDemoOrMapName();
            if (GameVideoRecorder::Instance().StartStudioRender(name, scrW, scrH))
            {
                GameVideoRecorder::Instance().SetDemoPaused(false);
                gEngfuncs.pfnClientCmd("dem_pause 0\n");
                gEngfuncs.pfnClientCmd("dem_speed 10.0\n");
                gEngfuncs.pfnClientCmd("fps_override 1\n");
                gEngfuncs.pfnClientCmd("fps_max 0\n");
                gEngfuncs.pfnClientCmd("gl_vsync 0\n");
                gEngfuncs.pfnClientCmd("host_framerate 0.01000000\n");
            }
            else
            {
                GameVideoRecorder::Instance().DiscardHighlight();
                GameVideoRecorder::Instance().SetDemoPaused(false);
                gEngfuncs.pfnClientCmd("dem_pause 0\n");
            }
        }
    }
    else if (GameVideoRecorder::Instance().IsHighlightRendering())
    {
        GameVideoRecorder::Instance().CaptureStudioFrame(scrW, scrH);

        if (GameVideoRecorder::Instance().GetRenderFramesPushed() >= GameVideoRecorder::Instance().GetRenderTargetFrames())
        {
            GameVideoRecorder::Instance().FinishStudioRender();
            gEngfuncs.pfnClientCmd("dem_speed 1.0\n");
            gEngfuncs.pfnClientCmd("host_framerate 0\n");
            gEngfuncs.pfnClientCmd("fps_override 0\n");
            gEngfuncs.pfnClientCmd("fps_max 100\n");
            gEngfuncs.pfnClientCmd("gl_vsync 1\n");
            GameVideoRecorder::Instance().SetDemoPaused(true);
            gEngfuncs.pfnClientCmd("dem_pause 1\n");

            g_HighlightToastText = "Studio Highlight saved: " + GameVideoRecorder::Instance().GetLastSavedHighlightPath();
            g_HighlightToastSuccess = true;
            g_HighlightToastTimer = gEngfuncs.GetClientTime() + 6.0;
        }
    }

    if (hud_draw_value != 0.0f && !overlay_visible && g_DemoMenuVisible)
        DrawDemoMenu();

    // Render top-right Windows capture style timer widget if match demo is recording
    if (GameVideoRecorder::Instance().IsMatchDemoRecording() && !g_DemoMenuVisible)
    {
        DrawWindowsCaptureWidget(scrW);
    }

    // Render Demo Highlight HUD elements during demo playback
    const bool isViewingDemo = (gEngfuncs.pDemoAPI && gEngfuncs.pDemoAPI->IsPlayingback());
    if (isViewingDemo)
    {
        FixDemoScoreboardTeams();
    }
    if (isViewingDemo && !overlay_visible)
    {
        if (GameVideoRecorder::Instance().IsHighlightSeeking() || GameVideoRecorder::Instance().IsHighlightRendering())
        {
            DrawHighlightRenderingModal(scrW, scrH);
        }
        else if (GameVideoRecorder::Instance().IsHighlightAwaitingConfirm())
        {
            DrawHighlightConfirmDialog(scrW, scrH);
        }
        else if (GameVideoRecorder::Instance().IsHighlightMarking())
        {
            DrawHighlightMarkingWidget(scrW);
        }
        else if (g_HighlightToastTimer > gEngfuncs.GetClientTime())
        {
            DrawHighlightToast(scrW, scrH);
        }
        else if (!g_DemoMenuVisible)
        {
            DrawHighlightIdleHint(scrW);
        }
    }

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

    if (GameVideoRecorder::Instance().IsMatchDemoRecording())
        GameVideoRecorder::Instance().StopMatchDemo();

    if (GameVideoRecorder::Instance().IsHighlightMarking() || 
        GameVideoRecorder::Instance().IsHighlightAwaitingConfirm() ||
        GameVideoRecorder::Instance().IsHighlightSeeking() ||
        GameVideoRecorder::Instance().IsHighlightRendering())
    {
        GameVideoRecorder::Instance().DiscardHighlight();
        GameVideoRecorder::Instance().SetDemoPaused(false);
        gEngfuncs.pfnClientCmd("dem_speed 1.0\n");
        gEngfuncs.pfnClientCmd("host_framerate 0\n");
        gEngfuncs.pfnClientCmd("fps_override 0\n");
        gEngfuncs.pfnClientCmd("fps_max 100\n");
        gEngfuncs.pfnClientCmd("gl_vsync 1\n");
    }
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
