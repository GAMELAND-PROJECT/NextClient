//========= Copyright Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerDialog.cpp: implementation of the CDemoPlayerDialog class.
//
//=============================================================================

#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include "DemoPlayerDialog.h"

#include <vgui/ISurfaceNext.h>
#include <vgui/ISchemeNext.h>
#include <KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/FileOpenDialog.h>

#include "GameUi.h"
#include "DemoPlayerFileDialog.h"
#include "DemoEventsDialog.h"

#include <basetypes.h>
#include <mathlib/mathlib.h>
#include <common.h>
#include <cvardef.h>
#include <pm_defs.h>
#include <kbutton.h>
#include <r_efx.h>
#include <r_studioint.h>

#include <custom.h>
#include <IWorld.h>
#include <IEngineWrapper.h>
#include <IDirector.h>

#ifdef _WIN32
#include <windows.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#undef PostMessage

#pragma warning(disable : 4244) // 'conversion' conversion from 'type1' to 'type2', possible loss of data

using namespace vgui2;

//////////////////////////////////////////////////////////////////////
// DemoPlayer Engine Hook: Eliminates Forward Jump Bug, Stream Overflow,
// Command Flooding, and Camera Spinning
//////////////////////////////////////////////////////////////////////

typedef void (__fastcall *SetWorldTime_fn)(IDemoPlayer *pThis, void *edx, double time, bool relative);
typedef int (__fastcall *ReadDemoMessage_fn)(IDemoPlayer *pThis, void *edx, unsigned char *buffer, int size);
typedef void (__fastcall *GetDemoViewInfo_fn)(IDemoPlayer *pThis, void *edx, ref_params_t *rp, float *view, int *viewmodel);

static SetWorldTime_fn g_pfnOrigSetWorldTime = nullptr;
static ReadDemoMessage_fn g_pfnOrigReadDemoMessage = nullptr;
static GetDemoViewInfo_fn g_pfnOrigGetDemoViewInfo = nullptr;
static void **g_pDemoPlayerVTable = nullptr;

static void __fastcall Hooked_SetWorldTime(IDemoPlayer *pThis, void *edx, double time, bool relative)
{
    if (!pThis)
        return;

    uint8_t *base = reinterpret_cast<uint8_t*>(pThis);
    double curWorldTime = *reinterpret_cast<double*>(base + 0x3A8);
    double targetTime = relative ? (curWorldTime + time) : time;

    IWorld *pWorld = pThis->GetWorld();
    if (pWorld)
    {
        frame_t *firstFrame = pWorld->GetFirstFrame();
        frame_t *lastFrame = pWorld->GetLastFrame();
        if (firstFrame && lastFrame && lastFrame->time > firstFrame->time)
        {
            targetTime = std::clamp(targetTime, static_cast<double>(firstFrame->time), static_cast<double>(lastFrame->time));
        }

        frame_t *targetFrame = pWorld->GetFrameByTime(static_cast<float>(targetTime));
        if (targetFrame)
        {
            uint32_t targetSeq = targetFrame->seqnr;
            uint32_t lastSeq = (targetSeq > 1) ? (targetSeq - 1) : 0;

            *reinterpret_cast<uint32_t*>(base + 0x3D8) = lastSeq;   // m_LastFrameSeqNr
            *reinterpret_cast<uint32_t*>(base + 0x3DC) = 0;         // m_DeltaFrameSeqNr = 0 (forces clean full keyframe)
            *reinterpret_cast<double*>(base + 0x3A8)   = targetTime; // m_WorldTime
            *reinterpret_cast<double*>(base + 0x3C0)   = targetTime; // m_LastFrameTime
            return;
        }
    }

    if (g_pfnOrigSetWorldTime)
    {
        g_pfnOrigSetWorldTime(pThis, edx, time, relative);
    }
}

static int __fastcall Hooked_ReadDemoMessage(IDemoPlayer *pThis, void *edx, unsigned char *buffer, int size)
{
    if (!pThis || !g_pfnOrigReadDemoMessage)
        return 0;

    uint8_t *base = reinterpret_cast<uint8_t*>(pThis);
    int playerState = *reinterpret_cast<int*>(base + 0x37C); // DEMOPLAYER_RUNNING == 4

    if (playerState == 4) // DEMOPLAYER_RUNNING
    {
        double worldTime = *reinterpret_cast<double*>(base + 0x3A8);
        uint32_t lastFrameSeqNr = *reinterpret_cast<uint32_t*>(base + 0x3D8);

        IWorld *pWorld = pThis->GetWorld();
        if (pWorld && lastFrameSeqNr != 0)
        {
            frame_t *curFrame = pWorld->GetFrameByTime(static_cast<float>(worldTime));
            if (curFrame)
            {
                if (curFrame->seqnr > lastFrameSeqNr + 2 || curFrame->seqnr < lastFrameSeqNr)
                {
                    uint32_t targetSeq = curFrame->seqnr;
                    uint32_t lastSeq = (targetSeq > 1) ? (targetSeq - 1) : 0;

                    *reinterpret_cast<uint32_t*>(base + 0x3D8) = lastSeq;   // m_LastFrameSeqNr
                    *reinterpret_cast<uint32_t*>(base + 0x3DC) = 0;         // m_DeltaFrameSeqNr (full keyframe)
                    *reinterpret_cast<double*>(base + 0x3C0)   = worldTime; // m_LastFrameTime
                }
            }
        }
    }

    return g_pfnOrigReadDemoMessage(pThis, edx, buffer, size);
}

static void __fastcall Hooked_GetDemoViewInfo(IDemoPlayer *pThis, void *edx, ref_params_t *rp, float *view, int *viewmodel)
{
    if (!pThis || !g_pfnOrigGetDemoViewInfo)
        return;

    uint8_t *base = reinterpret_cast<uint8_t*>(pThis);
    double worldTime = *reinterpret_cast<double*>(base + 0x3A8);
    uint32_t lastFrameSeqNr = *reinterpret_cast<uint32_t*>(base + 0x3D8);

    IWorld *pWorld = pThis->GetWorld();
    if (pWorld && lastFrameSeqNr != 0)
    {
        frame_t *curFrame = pWorld->GetFrameByTime(static_cast<float>(worldTime));
        if (curFrame && (curFrame->seqnr > lastFrameSeqNr + 2 || curFrame->seqnr < lastFrameSeqNr))
        {
            uint32_t targetSeq = curFrame->seqnr;
            uint32_t lastSeq = (targetSeq > 1) ? (targetSeq - 1) : 0;
            *reinterpret_cast<uint32_t*>(base + 0x3D8) = lastSeq;
            *reinterpret_cast<uint32_t*>(base + 0x3DC) = 0;
            *reinterpret_cast<double*>(base + 0x3C0)   = worldTime;
        }
    }

    g_pfnOrigGetDemoViewInfo(pThis, edx, rp, view, viewmodel);
}

static void InstallDemoPlayerHooks(IDemoPlayer *pDemoPlayer)
{
    if (!pDemoPlayer || g_pDemoPlayerVTable)
        return;

    void **vtable = *reinterpret_cast<void***>(pDemoPlayer);
    if (!vtable)
        return;

    g_pDemoPlayerVTable = vtable;

    DWORD oldProtect;
    if (VirtualProtect(&vtable[22], sizeof(void*), PAGE_READWRITE, &oldProtect))
    {
        g_pfnOrigSetWorldTime = reinterpret_cast<SetWorldTime_fn>(vtable[22]);
        vtable[22] = reinterpret_cast<void*>(&Hooked_SetWorldTime);
        VirtualProtect(&vtable[22], sizeof(void*), oldProtect, &oldProtect);
    }

    if (VirtualProtect(&vtable[44], sizeof(void*), PAGE_READWRITE, &oldProtect))
    {
        g_pfnOrigGetDemoViewInfo = reinterpret_cast<GetDemoViewInfo_fn>(vtable[44]);
        vtable[44] = reinterpret_cast<void*>(&Hooked_GetDemoViewInfo);
        VirtualProtect(&vtable[44], sizeof(void*), oldProtect, &oldProtect);
    }

    if (VirtualProtect(&vtable[45], sizeof(void*), PAGE_READWRITE, &oldProtect))
    {
        g_pfnOrigReadDemoMessage = reinterpret_cast<ReadDemoMessage_fn>(vtable[45]);
        vtable[45] = reinterpret_cast<void*>(&Hooked_ReadDemoMessage);
        VirtualProtect(&vtable[45], sizeof(void*), oldProtect, &oldProtect);
    }
}

static void UninstallDemoPlayerHooks()
{
    if (!g_pDemoPlayerVTable)
        return;

    DWORD oldProtect;
    if (g_pfnOrigSetWorldTime && VirtualProtect(&g_pDemoPlayerVTable[22], sizeof(void*), PAGE_READWRITE, &oldProtect))
    {
        g_pDemoPlayerVTable[22] = reinterpret_cast<void*>(g_pfnOrigSetWorldTime);
        VirtualProtect(&g_pDemoPlayerVTable[22], sizeof(void*), oldProtect, &oldProtect);
        g_pfnOrigSetWorldTime = nullptr;
    }

    if (g_pfnOrigGetDemoViewInfo && VirtualProtect(&g_pDemoPlayerVTable[44], sizeof(void*), PAGE_READWRITE, &oldProtect))
    {
        g_pDemoPlayerVTable[44] = reinterpret_cast<void*>(g_pfnOrigGetDemoViewInfo);
        VirtualProtect(&g_pDemoPlayerVTable[44], sizeof(void*), oldProtect, &oldProtect);
        g_pfnOrigGetDemoViewInfo = nullptr;
    }

    if (g_pfnOrigReadDemoMessage && VirtualProtect(&g_pDemoPlayerVTable[45], sizeof(void*), PAGE_READWRITE, &oldProtect))
    {
        g_pDemoPlayerVTable[45] = reinterpret_cast<void*>(g_pfnOrigReadDemoMessage);
        VirtualProtect(&g_pDemoPlayerVTable[45], sizeof(void*), oldProtect, &oldProtect);
        g_pfnOrigReadDemoMessage = nullptr;
    }

    g_pDemoPlayerVTable = nullptr;
}

CDemoPlayerDialog::CDemoPlayerDialog(vgui2::Panel *parent) : Frame(parent, "DemoPlayerDialog")
{
    m_World = NULL;
    m_Engine = NULL;
    m_DemoPlayer = NULL;
    m_System = NULL;

    // Completely suppress Demo Player UI during headless background video conversion
    if (strstr(GetCommandLineA(), "-democonvert") != nullptr)
    {
        SetVisible(false);
        return;
    }

    if (!LoadModules())
    {
        SetVisible(false);
        return;
    }

    int screenW = 800, screenH = 600;
    if (vgui2::surface())
    {
        vgui2::surface()->GetScreenSize(screenW, screenH);
    }
    const int dlgW = 600;
    const int dlgH = 94;
    SetBounds((screenW - dlgW) / 2, screenH - dlgH - 45, dlgW, dlgH);

    SetSizeable(false);
    SetMoveable(true);

    vgui2::surface()->CreatePopup(GetVPanel(), false);
    SetVisible(true);
    SetTitle("#GameUI_DemoPlayer", true);

    // Modern read-only smooth progress gauge: non-clickable, non-draggable
    m_pProgressBar = new ContinuousProgressBar(this, "ProgressBar");
    m_pProgressBar->SetProgress(0.0f);

    m_pLableTimeCode = new Label(this, "TimeLabel", "00:00:00 / 00:00");

    // Standard demo controls
    m_pButtonLoad = new Button(this, "LoadButton", "");
    m_pButtonStart = new Button(this, "StartButton", "");
    m_pButtonStepB = new Button(this, "StepBButton", "");
    m_pButtonPause = new Button(this, "PauseButton", "");
    m_pButtonPlay = new Button(this, "PlayButton", "");
    m_pButtonStepF = new Button(this, "StepFButton", "");
    m_pButtonEnd = new Button(this, "EndButton", "");
    m_pButtonStop = new Button(this, "StopButton", "");

    // Round Navigation & Instant Replay
    m_pButtonPrevRound = new Button(this, "PrevRoundBtn", "|<< Rnd");
    m_pButtonNextRound = new Button(this, "NextRoundBtn", "Rnd >>|");
    m_pButtonJumpBack = new Button(this, "JumpBackBtn", "-5s");

    m_pButtonPrevRound->SetCommand("prevround");
    m_pButtonNextRound->SetCommand("nextround");
    m_pButtonJumpBack->SetCommand("jumpback5");

    // Multi-speed presets
    m_pButtonSpeedHalf = new Button(this, "SpeedHalfBtn", "0.5x");
    m_pButtonSpeed1x = new Button(this, "Speed1xBtn", "1.0x");
    m_pButtonSpeed2x = new Button(this, "Speed2xBtn", "2x");
    m_pButtonSpeed4x = new Button(this, "Speed4xBtn", "4x");
    m_pButtonSpeed8x = new Button(this, "Speed8xBtn", "8x");
    m_pButtonSpeed16x = new Button(this, "Speed16xBtn", "16x");

    m_pButtonSpeedHalf->SetCommand("speed_0.5");
    m_pButtonSpeed1x->SetCommand("speed_1.0");
    m_pButtonSpeed2x->SetCommand("speed_2.0");
    m_pButtonSpeed4x->SetCommand("speed_4.0");
    m_pButtonSpeed8x->SetCommand("speed_8.0");
    m_pButtonSpeed16x->SetCommand("speed_16.0");

    // Toggle and Editor buttons
    m_MasterButton = new ToggleButton(this, "MasterButton", "Master");
    m_MasterButton->AddActionSignalTarget(this);
    m_MasterButton->SetVisible(false);

    m_NextTimeScale = 2.0f;
    m_bIsSeeking = false;
    m_targetSeekTime = 0.0;
    m_demoStartTime = 0.0;
    m_demoEndTime = 0.0;
    m_lastLoadedDemo.clear();
    m_bFastForwardingToNextRound = false;
    m_targetRoundTime = 0.0;
    m_RoundStartTimes.clear();

    LoadControlSettings("Resource\\DemoPlayerDialog.res");
    LoadUserConfig("DemoPlayerDialog");

    // Completely hide legacy TimeSlider from .res if present
    Panel *pLegacySlider = FindChildByName("TimeSlider");
    if (pLegacySlider)
    {
        pLegacySlider->SetVisible(false);
        pLegacySlider->SetEnabled(false);
    }

    m_hDemoEventsDialog = new CDemoEventsDialog(this, "DemoEventsDialog", m_Engine, m_DemoPlayer);
    m_hDemoEventsDialog->AddActionSignalTarget(this);
    m_hDemoPlayerFileDialog = NULL;
}

CDemoPlayerDialog::~CDemoPlayerDialog()
{
    UninstallDemoPlayerHooks();
}

void CDemoPlayerDialog::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    Panel *pLegacySlider = FindChildByName("TimeSlider");
    if (pLegacySlider)
    {
        pLegacySlider->SetVisible(false);
        pLegacySlider->SetEnabled(false);
    }

    if (scheme())
    {
        if (m_pButtonLoad) m_pButtonLoad->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_load", false), 0);
        if (m_pButtonPlay) m_pButtonPlay->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_play", false), 0);
        if (m_pButtonPause) m_pButtonPause->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_pause", false), 0);
        if (m_pButtonStepF) m_pButtonStepF->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_stepf", false), 0);
        if (m_pButtonStepB) m_pButtonStepB->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_stepb", false), 0);
        if (m_pButtonStart) m_pButtonStart->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_start", false), 0);
        if (m_pButtonEnd) m_pButtonEnd->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_end", false), 0);
        if (m_pButtonStop) m_pButtonStop->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_stop", false), 0);
    }

    const int pad = 8;
    const int dlgW = GetWide();
    const int progressY = 28;
    const int progressH = 18;
    const int btnY = 54;
    const int btnH = 26;

    // Row 1: Smooth continuous progress bar + TimeCode / Round status
    const int labelW = 210;
    const int barW = dlgW - (pad * 2) - labelW - 6;

    if (m_pProgressBar)
    {
        m_pProgressBar->SetBounds(pad, progressY, barW, progressH);
    }

    if (m_pLableTimeCode)
    {
        m_pLableTimeCode->SetBounds(pad + barW + 6, progressY, labelW, progressH);
    }

    // Row 2: Playback Controls and Multi-Speed Presets
    int curX = pad;
    auto placeBtn = [&](Button *btn, int width) {
        if (btn)
        {
            btn->SetBounds(curX, btnY, width, btnH);
            curX += width + 3;
        }
    };

    placeBtn(m_pButtonLoad, 24);
    placeBtn(m_pButtonStart, 24);
    placeBtn(m_pButtonPrevRound, 54);
    placeBtn(m_pButtonJumpBack, 32);
    placeBtn(m_pButtonPause, 24);
    placeBtn(m_pButtonPlay, 24);
    placeBtn(m_pButtonStepF, 22);
    placeBtn(m_pButtonNextRound, 54);

    curX += 6; // separator spacing before speed controls

    placeBtn(m_pButtonSpeedHalf, 34);
    placeBtn(m_pButtonSpeed1x, 34);
    placeBtn(m_pButtonSpeed2x, 26);
    placeBtn(m_pButtonSpeed4x, 26);
    placeBtn(m_pButtonSpeed8x, 26);
    placeBtn(m_pButtonSpeed16x, 34);

    if (m_pButtonStop)
    {
        m_pButtonStop->SetBounds(dlgW - pad - 24, btnY, 24, btnH);
    }
}

void CDemoPlayerDialog::OnThink()
{
    BaseClass::OnThink();

    if (!m_DemoPlayer || !m_DemoPlayer->IsActive())
        return;

    Update();
}

void CDemoPlayerDialog::PerformSeek(double targetTime)
{
    if (!m_DemoPlayer)
        return;

    if (m_demoEndTime > m_demoStartTime)
    {
        targetTime = std::clamp(targetTime, m_demoStartTime, m_demoEndTime);
    }
    else if (targetTime < 0.0)
    {
        targetTime = 0.0;
    }

    m_targetSeekTime = targetTime;
    m_bIsSeeking = true;
    m_seekStartTime = std::chrono::steady_clock::now();

    UpdateTimeCodeLabel(targetTime, true);

    m_DemoPlayer->SetWorldTime(targetTime, false);

    if (m_Engine)
    {
        m_Engine->Cbuf_AddText("stopsound\n");
    }
}

void CDemoPlayerDialog::ScanRoundBookmarks()
{
    m_RoundStartTimes.clear();

    if (!m_World)
        return;

    frame_t *firstFrame = m_World->GetFirstFrame();
    frame_t *lastFrame = m_World->GetLastFrame();
    if (!firstFrame || !lastFrame || lastFrame->time <= firstFrame->time)
        return;

    char szRoundTime[] = "RoundTime";
    char szResetHUD[] = "ResetHUD";
    int roundTimeMsg = m_World->FindUserMsgByName(szRoundTime);
    int resetHudMsg = m_World->FindUserMsgByName(szResetHUD);

    m_RoundStartTimes.push_back(m_demoStartTime);
    double lastRoundTime = m_demoStartTime;

    for (unsigned int seq = firstFrame->seqnr; seq <= lastFrame->seqnr; ++seq)
    {
        frame_t *f = m_World->GetFrameBySeqNr(seq);
        if (!f || f->time <= m_demoStartTime)
            continue;

        bool isRoundEvent = false;
        if (f->userMessages && f->userMessagesSize > 0)
        {
            if (roundTimeMsg > 0 || resetHudMsg > 0)
            {
                for (unsigned int i = 0; i < f->userMessagesSize; ++i)
                {
                    unsigned char b = f->userMessages[i];
                    if ((roundTimeMsg > 0 && b == static_cast<unsigned char>(roundTimeMsg)) ||
                        (resetHudMsg > 0 && b == static_cast<unsigned char>(resetHudMsg)))
                    {
                        isRoundEvent = true;
                        break;
                    }
                }
            }
        }

        if (isRoundEvent && (f->time - lastRoundTime) >= 30.0)
        {
            m_RoundStartTimes.push_back(f->time);
            lastRoundTime = f->time;
        }
    }
}

void CDemoPlayerDialog::UpdateTimeCodeLabel(double worldTime, bool isSeeking)
{
    if (!m_pLableTimeCode)
        return;

    double relativeTime = std::max(0.0, worldTime - m_demoStartTime);
    double totalDuration = std::max(0.0, m_demoEndTime - m_demoStartTime);

    int curSec = static_cast<int>(relativeTime);
    int curMin = curSec / 60;
    curSec %= 60;
    int curMsec = static_cast<int>(std::fmod(relativeTime, 1.0) * 100.0);
    if (curMsec < 0) curMsec = 0;

    int totalSec = static_cast<int>(totalDuration);
    int totalMin = totalSec / 60;
    totalSec %= 60;

    int currentRound = 1;
    int totalRounds = static_cast<int>(m_RoundStartTimes.size());
    if (totalRounds > 1)
    {
        for (int i = 0; i < totalRounds; ++i)
        {
            if (worldTime >= m_RoundStartTimes[i])
            {
                currentRound = i + 1;
            }
        }
    }

    char buf[80]{};
    if (totalRounds > 1)
    {
        std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d / %02d:%02d [Rnd %d/%d]",
            curMin, curSec, curMsec, totalMin, totalSec, currentRound, totalRounds);
    }
    else
    {
        int pct = (totalDuration > 0.0) ? static_cast<int>(std::clamp((relativeTime / totalDuration) * 100.0, 0.0, 100.0)) : 0;
        std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d / %02d:%02d (%d%%)",
            curMin, curSec, curMsec, totalMin, totalSec, pct);
    }
    m_pLableTimeCode->SetText(buf);
}

void CDemoPlayerDialog::OnJumpRelative(double deltaSeconds)
{
    if (!m_DemoPlayer)
        return;

    m_bFastForwardingToNextRound = false;
    double baseTime = m_bIsSeeking ? m_targetSeekTime : m_DemoPlayer->GetWorldTime();
    double targetTime = baseTime + deltaSeconds;
    PerformSeek(targetTime);
}

void CDemoPlayerDialog::OnSetSpeed(float speed)
{
    if (!m_DemoPlayer)
        return;

    m_bFastForwardingToNextRound = false;
    if (m_DemoPlayer->IsPaused())
    {
        m_DemoPlayer->SetPaused(false);
    }
    m_DemoPlayer->SetTimeScale(speed);
}

void CDemoPlayerDialog::OnPrevRound()
{
    if (!m_DemoPlayer || !m_World)
        return;

    m_bFastForwardingToNextRound = false;
    double curTime = m_DemoPlayer->GetWorldTime();

    double targetRound = m_demoStartTime;
    for (int i = static_cast<int>(m_RoundStartTimes.size()) - 1; i >= 0; --i)
    {
        double rTime = m_RoundStartTimes[i];
        if (rTime <= curTime - 4.0)
        {
            targetRound = rTime;
            break;
        }
        else if (rTime < curTime && i > 0)
        {
            targetRound = m_RoundStartTimes[i - 1];
            break;
        }
    }

    PerformSeek(targetRound);
    m_DemoPlayer->SetTimeScale(1.0f);
}

void CDemoPlayerDialog::OnNextRound()
{
    if (!m_DemoPlayer || !m_World)
        return;

    double curTime = m_DemoPlayer->GetWorldTime();
    double targetRound = 0.0;

    for (double rTime : m_RoundStartTimes)
    {
        if (rTime > curTime + 1.5)
        {
            targetRound = rTime;
            break;
        }
    }

    if (targetRound > 0.0)
    {
        m_bFastForwardingToNextRound = true;
        m_targetRoundTime = targetRound;
        if (m_DemoPlayer->IsPaused())
        {
            m_DemoPlayer->SetPaused(false);
        }
        m_DemoPlayer->SetTimeScale(16.0f);
    }
    else
    {
        m_bFastForwardingToNextRound = false;
        if (m_DemoPlayer->IsPaused())
        {
            m_DemoPlayer->SetPaused(false);
        }
        m_DemoPlayer->SetTimeScale(16.0f);
    }
}

void CDemoPlayerDialog::Update()
{
    if (!m_DemoPlayer || !m_World)
        return;

    if (strstr(GetCommandLineA(), "-democonvert") != nullptr)
    {
        SetVisible(false);
        return;
    }

    wchar_t title[300]{};
    if (!m_DemoPlayer->IsActive())
    {
        SetTitle("Demo Player", false);
    }
    else if (m_DemoPlayer->IsLoading())
    {
        swprintf(title, L"Loading %hs ...", m_DemoPlayer->GetFileName());
        SetTitle(title, false);
    }
    else
    {
        swprintf(title, L"Demo Player - %hs", m_DemoPlayer->GetFileName());
        SetTitle(title, false);
    }

    // Sync time boundaries by skipping pre-game frames before the actual start of the demo
    if (m_World)
    {
        frame_t *firstFrame = m_World->GetFirstFrame();
        frame_t *lastFrame = m_World->GetLastFrame();

        if (firstFrame && lastFrame && lastFrame->time > firstFrame->time)
        {
            const char *curDemo = m_DemoPlayer->GetFileName();
            if (curDemo && (m_lastLoadedDemo != curDemo || m_demoEndTime <= m_demoStartTime || lastFrame->time > m_demoEndTime))
            {
                frame_t *prevFrame = NULL;
                frame_t *nextFrame = firstFrame;
                int searchCount = 0;

                while (nextFrame && searchCount < 100)
                {
                    if (prevFrame && (nextFrame->time - prevFrame->time) > 2.0f)
                    {
                        firstFrame = nextFrame;
                        break;
                    }

                    prevFrame = nextFrame;
                    nextFrame = m_World->GetFrameBySeqNr(nextFrame->seqnr + 1);
                    searchCount++;
                }

                if (lastFrame->time > firstFrame->time)
                {
                    m_lastLoadedDemo = curDemo ? curDemo : "";
                    m_demoStartTime = firstFrame->time;
                    m_demoEndTime = lastFrame->time;
                    m_bIsSeeking = false;
                    ScanRoundBookmarks();
                }
            }
        }
    }

    double worldTime = m_DemoPlayer->GetWorldTime();

    // Check seeking completion or timeout
    if (m_bIsSeeking)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_seekStartTime).count();

        if (std::abs(worldTime - m_targetSeekTime) <= 0.4 || elapsedMs >= 600)
        {
            m_bIsSeeking = false;
        }
    }

    // Automatic Next Round fast-forward tracking:
    if (m_bFastForwardingToNextRound)
    {
        if (worldTime >= m_targetRoundTime - 0.15 || !m_DemoPlayer->IsActive() || m_DemoPlayer->IsPaused())
        {
            m_bFastForwardingToNextRound = false;
            m_DemoPlayer->SetTimeScale(1.0f);
        }
    }

    // Update progress bar
    if (m_pProgressBar)
    {
        float progress = 0.0f;
        double totalDuration = m_demoEndTime - m_demoStartTime;
        if (totalDuration > 0.0)
        {
            double displayTime = m_bIsSeeking ? m_targetSeekTime : worldTime;
            progress = static_cast<float>(std::clamp((displayTime - m_demoStartTime) / totalDuration, 0.0, 1.0));
        }
        m_pProgressBar->SetProgress(progress);
    }

    // Update TimeCode label
    UpdateTimeCodeLabel(m_bIsSeeking ? m_targetSeekTime : worldTime, m_bIsSeeking);

    // Update Next Round button indicator
    if (m_pButtonNextRound)
    {
        m_pButtonNextRound->SetText(m_bFastForwardingToNextRound ? ">> Rnd..." : "Rnd >>|");
    }

    // Highlight active speed preset
    float scale = m_DemoPlayer->GetTimeScale();
    if (m_pButtonSpeedHalf)
        m_pButtonSpeedHalf->SetText(std::abs(scale - 0.5f) < 0.1f ? "[0.5x]" : "0.5x");
    if (m_pButtonSpeed1x)
        m_pButtonSpeed1x->SetText(std::abs(scale - 1.0f) < 0.1f ? "[1.0x]" : "1.0x");
    if (m_pButtonSpeed2x)
        m_pButtonSpeed2x->SetText(std::abs(scale - 2.0f) < 0.1f ? "[2x]" : "2x");
    if (m_pButtonSpeed4x)
        m_pButtonSpeed4x->SetText(std::abs(scale - 4.0f) < 0.1f ? "[4x]" : "4x");
    if (m_pButtonSpeed8x)
        m_pButtonSpeed8x->SetText(std::abs(scale - 8.0f) < 0.1f ? "[8x]" : "8x");
    if (m_pButtonSpeed16x)
        m_pButtonSpeed16x->SetText(std::abs(scale - 16.0f) < 0.1f ? "[16x]" : "16x");
}

void CDemoPlayerDialog::OnPlay()
{
    if (!m_DemoPlayer) return;
    m_bFastForwardingToNextRound = false;
    m_DemoPlayer->SetPaused(false);
    if (m_Engine)
    {
        m_Engine->SetCvar("spec_autodirector", "1");
    }
}

bool CDemoPlayerDialog::LoadModules()
{
    m_System = SystemWrapper();
    if (m_System == NULL)
        return false;

    m_Engine = (IEngineWrapper*)m_System->GetModule("enginewrapper002", "", NULL);
    if (m_Engine == NULL)
    {
        m_System->Printf("CDemoPlayerDialog::LoadModules: couldn't get engine interface.\n");
        return false;
    }

    m_DemoPlayer = (IDemoPlayer*)m_System->GetModule(DEMOPLAYER_INTERFACE_VERSION, "", NULL);
    if (m_DemoPlayer == NULL)
    {
        m_System->Printf("CDemoPlayerDialog::LoadModules: couldn't load demo player module.\n");
        return false;
    }

    m_World = m_DemoPlayer->GetWorld();
    if (m_World == NULL)
    {
        m_System->Printf("CDemoPlayerDialog::LoadModules: couldn't get world module.\n");
        return false;
    }

    InstallDemoPlayerHooks(m_DemoPlayer);

    return true;
}

void CDemoPlayerDialog::OnPause()
{
    if (!m_DemoPlayer) return;
    m_bFastForwardingToNextRound = false;
    m_DemoPlayer->SetPaused(true);
    if (m_Engine)
    {
        m_Engine->Cbuf_AddText("stopsound\n");
    }
}

void CDemoPlayerDialog::OnNextFrame(int direction)
{
    if (!m_DemoPlayer || !m_World) return;

    m_bFastForwardingToNextRound = false;
    double time = m_bIsSeeking ? m_targetSeekTime : m_DemoPlayer->GetWorldTime();
    frame_t *frame = m_World->GetFrameByTime(static_cast<float>(time));
    if (!frame) return;

    frame = m_World->GetFrameBySeqNr(frame->seqnr + direction);
    if (!frame) return;

    if (frame->time < m_demoStartTime)
    {
        PerformSeek(m_demoStartTime);
    }
    else if (frame->time > m_demoEndTime)
    {
        PerformSeek(m_demoEndTime);
    }
    else
    {
        PerformSeek(frame->time);
    }
    m_DemoPlayer->SetPaused(true);
}

void CDemoPlayerDialog::OnStart()
{
    if (!m_DemoPlayer || !m_World) return;

    m_bFastForwardingToNextRound = false;
    PerformSeek(m_demoStartTime);
    m_DemoPlayer->SetTimeScale(1.0f);
    m_DemoPlayer->SetPaused(true);
}

void CDemoPlayerDialog::OnEnd()
{
    if (!m_DemoPlayer || !m_World) return;

    m_bFastForwardingToNextRound = false;
    PerformSeek(m_demoEndTime);
    m_DemoPlayer->SetPaused(true);
}

void CDemoPlayerDialog::OnSlower()
{
    if (!m_DemoPlayer) return;
    m_bFastForwardingToNextRound = false;
    float curScale = m_DemoPlayer->GetTimeScale();
    float newScale = 1.0f;

    if (curScale > 3.0f) newScale = 2.0f;
    else if (curScale > 1.5f) newScale = 1.0f;
    else if (curScale > 0.75f) newScale = 0.5f;
    else if (curScale > 0.35f) newScale = 0.25f;
    else newScale = 0.25f;

    m_DemoPlayer->SetTimeScale(newScale);
}

void CDemoPlayerDialog::OnFaster()
{
    if (!m_DemoPlayer) return;
    m_bFastForwardingToNextRound = false;
    float curScale = m_DemoPlayer->GetTimeScale();
    float newScale = 1.0f;

    if (curScale < 0.35f) newScale = 0.5f;
    else if (curScale < 0.75f) newScale = 1.0f;
    else if (curScale < 1.5f) newScale = 2.0f;
    else if (curScale < 3.0f) newScale = 4.0f;
    else newScale = 4.0f;

    m_DemoPlayer->SetTimeScale(newScale);
}

void CDemoPlayerDialog::OnResetSpeed()
{
    if (!m_DemoPlayer) return;
    m_bFastForwardingToNextRound = false;
    m_DemoPlayer->SetTimeScale(1.0f);
}

void CDemoPlayerDialog::OnSave()
{
    if (m_DemoPlayer)
    {
        m_DemoPlayer->SaveGame("demoedit.dem");
    }
}

void CDemoPlayerDialog::OnEvents()
{
    if (!m_hDemoEventsDialog.Get())
    {
        m_hDemoEventsDialog = new CDemoEventsDialog(this, "DemoEventsDialog", m_Engine, m_DemoPlayer);
        m_hDemoEventsDialog->AddActionSignalTarget(this);
    }
    m_hDemoEventsDialog->Activate();
    PostMessage(m_hDemoEventsDialog->GetVPanel(), new KeyValues("UpdateCmdList"));
}

void CDemoPlayerDialog::OnClose()
{
    BaseClass::OnClose();
}

void CDemoPlayerDialog::OnCommand(const char *command)
{
    if (!m_DemoPlayer || !m_World || !m_Engine)
    {
        BaseClass::OnCommand(command);
        return;
    }

    if (!strcmp(command, "pause"))
    {
        OnPause();
    }
    else if (!strcmp(command, "load"))
    {
        OnLoad();
    }
    else if (!strcmp(command, "play"))
    {
        OnPlay();
    }
    else if (!strcmp(command, "stepf"))
    {
        OnNextFrame(1);
    }
    else if (!strcmp(command, "stepb"))
    {
        OnNextFrame(-1);
    }
    else if (!strcmp(command, "start"))
    {
        OnStart();
    }
    else if (!strcmp(command, "end"))
    {
        OnEnd();
    }
    else if (!strcmp(command, "slower"))
    {
        OnSlower();
    }
    else if (!strcmp(command, "faster"))
    {
        OnFaster();
    }
    else if (!strcmp(command, "speedreset"))
    {
        OnResetSpeed();
    }
    else if (!strcmp(command, "jumpback5"))
    {
        OnJumpRelative(-5.0);
    }
    else if (!strcmp(command, "prevround"))
    {
        OnPrevRound();
    }
    else if (!strcmp(command, "nextround"))
    {
        OnNextRound();
    }
    else if (!strcmp(command, "speed_0.5"))
    {
        OnSetSpeed(0.5f);
    }
    else if (!strcmp(command, "speed_1.0"))
    {
        OnSetSpeed(1.0f);
    }
    else if (!strcmp(command, "speed_2.0"))
    {
        OnSetSpeed(2.0f);
    }
    else if (!strcmp(command, "speed_4.0"))
    {
        OnSetSpeed(4.0f);
    }
    else if (!strcmp(command, "speed_8.0"))
    {
        OnSetSpeed(8.0f);
    }
    else if (!strcmp(command, "speed_16.0"))
    {
        OnSetSpeed(16.0f);
    }
    else if (!strcmp(command, "stop"))
    {
        OnStop();
    }
    else if (!strcmp(command, "events"))
    {
        OnEvents();
    }
    else if (!strcmp(command, "save"))
    {
        OnSave();
    }

    BaseClass::OnCommand(command);
}

void CDemoPlayerDialog::ReceiveSignal(ISystemModule *module, unsigned int signal)
{
    if (!m_DemoPlayer)
        return;

    if (module->GetSerial() == m_DemoPlayer->GetSerial())
    {
        switch (signal)
        {
            case DIRECTOR_SIGNAL_UPDATE:
                if (m_DemoPlayer->IsEditMode() && m_hDemoEventsDialog.Get())
                {
                    PostMessage(m_hDemoEventsDialog->GetVPanel(), new KeyValues("UpdateCmdList"));
                }
                break;

            case DIRECTOR_SIGNAL_LASTCMD:
                if (m_DemoPlayer->IsEditMode() && m_hDemoEventsDialog.Get())
                {
                    PostMessage(m_hDemoEventsDialog->GetVPanel(), new KeyValues("UpdateLastCmd"));
                }
                break;

            case DIRECTOR_SIGNAL_SHUTDOWN:
                UninstallDemoPlayerHooks();
                m_DemoPlayer = NULL;
                break;

            default:
                if (m_System)
                {
                    m_System->Printf("CDemoPlayerDialog::ReceiveSignal: unknown signal %i.\n", signal);
                }
                break;
        }
    }
}

void CDemoPlayerDialog::DemoSelected(const char *demoname)
{
    if (!m_DemoPlayer || !m_Engine)
        return;

    char fullstring[270];
    sprintf(fullstring, "viewdemo \"%s\"\n", demoname);

    m_bIsSeeking = false;
    m_demoStartTime = 0.0;
    m_demoEndTime = 0.0;
    m_lastLoadedDemo.clear();
    m_bFastForwardingToNextRound = false;
    m_targetRoundTime = 0.0;
    m_RoundStartTimes.clear();

    m_DemoPlayer->Stop();
    m_Engine->Cbuf_AddText(fullstring);
}

void CDemoPlayerDialog::ButtonToggled(int state)
{
    if (m_DemoPlayer)
    {
        m_DemoPlayer->SetMasterMode(state);
    }
}

void CDemoPlayerDialog::OnStop()
{
    m_bIsSeeking = false;
    m_bFastForwardingToNextRound = false;
    if (m_DemoPlayer)
    {
        m_DemoPlayer->Stop();
    }
    if (m_Engine)
    {
        m_Engine->Cbuf_AddText("stopdemo\n");
    }
    Update();
}

void CDemoPlayerDialog::OnLoad()
{
    if (!m_hDemoPlayerFileDialog.Get())
    {
        m_hDemoPlayerFileDialog = new CDemoPlayerFileDialog(this, "DemoPlayerFileDialog");
        m_hDemoPlayerFileDialog->AddActionSignalTarget(this);
    }
    m_hDemoPlayerFileDialog->Activate();
}
