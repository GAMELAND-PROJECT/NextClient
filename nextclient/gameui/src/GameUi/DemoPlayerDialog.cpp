//========= Copyright Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerDialog.cpp: implementation of the CDemoPlayerDialog class.
//
//=============================================================================

#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "DemoPlayerDialog.h"

#include <vgui/ISurfaceNext.h>
#include <vgui/ISchemeNext.h>
#include <KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Slider.h>
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
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

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
    const int dlgW = 560;
    const int dlgH = 92;
    SetBounds((screenW - dlgW) / 2, screenH - dlgH - 45, dlgW, dlgH);

    SetSizeable(false);
    SetMoveable(true);

    vgui2::surface()->CreatePopup(GetVPanel(), false);
    SetVisible(true);
    SetTitle("#GameUI_DemoPlayer", true);

    // High precision normalized slider: 0 to 1000 for sub-second smooth scrubbing
    m_pTimeSlider = new Slider(this, "TimeSlider");
    m_pTimeSlider->SetRange(0, 1000);
    m_pTimeSlider->SetDragOnRepositionNob(true);
    m_pTimeSlider->AddActionSignalTarget(this);

    m_pLableTimeCode = new Label(this, "TimeLabel", "00:00:00");

    // Standard demo controls
    m_pButtonLoad = new Button(this, "LoadButton", "");
    m_pButtonStart = new Button(this, "StartButton", "");
    m_pButtonStepB = new Button(this, "StepBButton", "");
    m_pButtonPause = new Button(this, "PauseButton", "");
    m_pButtonPlay = new Button(this, "PlayButton", "");
    m_pButtonStepF = new Button(this, "StepFButton", "");
    m_pButtonEnd = new Button(this, "EndButton", "");
    m_pButtonSlower = new Button(this, "SlowerButton", "");
    m_pButtonFaster = new Button(this, "FasterButton", "");
    m_pButtonStop = new Button(this, "StopButton", "");

    // Enhanced jump & speed controls
    m_pButtonJumpBack = new Button(this, "JumpBackBtn", "-5s");
    m_pButtonJumpFwd = new Button(this, "JumpFwdBtn", "+5s");
    m_pButtonSpeedReset = new Button(this, "SpeedResetBtn", "1.0x");

    m_pButtonJumpBack->SetCommand("jumpback5");
    m_pButtonJumpFwd->SetCommand("jumpfwd5");
    m_pButtonSpeedReset->SetCommand("speedreset");

    // Toggle and Editor buttons
    m_MasterButton = new ToggleButton(this, "MasterButton", "Master");
    m_MasterButton->AddActionSignalTarget(this);
    m_MasterButton->SetVisible(false);

    m_NextTimeScale = 2.0f;
    m_lastSliderTime = -1;

    LoadControlSettings("Resource\\DemoPlayerDialog.res");
    LoadUserConfig("DemoPlayerDialog");

    m_hDemoEventsDialog = new CDemoEventsDialog(this, "DemoEventsDialog", m_Engine, m_DemoPlayer);
    m_hDemoEventsDialog->AddActionSignalTarget(this);
    m_hDemoPlayerFileDialog = NULL;
}

CDemoPlayerDialog::~CDemoPlayerDialog()
{
}

void CDemoPlayerDialog::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

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
        if (m_pButtonFaster) m_pButtonFaster->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_faster", false), 0);
        if (m_pButtonSlower) m_pButtonSlower->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_slower", false), 0);
    }

    // Precise, elegant layout
    const int pad = 10;
    const int dlgW = GetWide();
    const int sliderY = 28;
    const int sliderH = 24;
    const int btnY = 56;
    const int btnH = 26;

    if (m_pTimeSlider)
    {
        m_pTimeSlider->SetBounds(pad, sliderY, dlgW - (pad * 2), sliderH);
    }

    int curX = pad;
    auto placeBtn = [&](Button *btn, int width) {
        if (btn)
        {
            btn->SetBounds(curX, btnY, width, btnH);
            curX += width + 4;
        }
    };

    placeBtn(m_pButtonLoad, 24);
    placeBtn(m_pButtonStart, 24);
    placeBtn(m_pButtonJumpBack, 34);
    placeBtn(m_pButtonStepB, 24);
    placeBtn(m_pButtonPause, 26);
    placeBtn(m_pButtonPlay, 26);
    placeBtn(m_pButtonStepF, 24);
    placeBtn(m_pButtonJumpFwd, 34);
    placeBtn(m_pButtonEnd, 24);
    placeBtn(m_pButtonSlower, 24);
    placeBtn(m_pButtonSpeedReset, 38);
    placeBtn(m_pButtonFaster, 24);

    if (m_pLableTimeCode)
    {
        int labelW = (dlgW - pad - 28) - curX;
        if (labelW < 90) labelW = 90;
        m_pLableTimeCode->SetBounds(curX, btnY, labelW, btnH);
    }

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

double CDemoPlayerDialog::SliderPosToWorldTime(int pos)
{
    if (m_demoEndTime <= m_demoStartTime)
        return m_demoStartTime;

    double frac = std::clamp(static_cast<double>(pos) / 1000.0, 0.0, 1.0);
    return m_demoStartTime + frac * (m_demoEndTime - m_demoStartTime);
}

int CDemoPlayerDialog::WorldTimeToSliderPos(double worldTime)
{
    if (m_demoEndTime <= m_demoStartTime)
        return 0;

    double frac = (worldTime - m_demoStartTime) / (m_demoEndTime - m_demoStartTime);
    frac = std::clamp(frac, 0.0, 1.0);
    return static_cast<int>(std::round(frac * 1000.0));
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

    m_DemoPlayer->SetWorldTime(targetTime, false);

    if (m_Engine)
    {
        m_Engine->Cbuf_AddText("stopsound\n");
    }
}

void CDemoPlayerDialog::UpdateTimeCodeLabel(double worldTime, bool isSeeking)
{
    if (!m_pLableTimeCode)
        return;

    int curSec = std::max(0, static_cast<int>(worldTime));
    int curMin = curSec / 60;
    curSec %= 60;
    int curMsec = static_cast<int>(std::fmod(worldTime, 1.0) * 100.0);
    if (curMsec < 0) curMsec = 0;

    int totalSec = std::max(0, static_cast<int>(m_demoEndTime - m_demoStartTime));
    int totalMin = totalSec / 60;
    totalSec %= 60;

    char buf[64]{};
    if (isSeeking)
    {
        std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d / %02d:%02d [SEEK]",
            curMin, curSec, curMsec, totalMin, totalSec);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d / %02d:%02d",
            curMin, curSec, curMsec, totalMin, totalSec);
    }
    m_pLableTimeCode->SetText(buf);
}

void CDemoPlayerDialog::OnSliderDragStart(int position)
{
    m_bUserIsDraggingSlider = true;
    m_bWasPlayingBeforeDrag = (m_DemoPlayer && !m_DemoPlayer->IsPaused());

    if (m_DemoPlayer)
    {
        m_DemoPlayer->SetPaused(true);
    }
    if (m_Engine)
    {
        m_Engine->Cbuf_AddText("stopsound\n");
    }
}

void CDemoPlayerDialog::OnSliderMoved(int position)
{
    if (!m_bUserIsDraggingSlider && m_pTimeSlider && m_pTimeSlider->IsDragged())
    {
        OnSliderDragStart(position);
    }

    if (!m_bUserIsDraggingSlider)
        return;

    double targetTime = SliderPosToWorldTime(position);
    UpdateTimeCodeLabel(targetTime, true);

    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSeekTime).count();
    if (elapsedMs >= 120) // Throttle to prevent flooding the engine while scrubbing
    {
        m_lastSeekTime = now;
        PerformSeek(targetTime);
    }
}

void CDemoPlayerDialog::OnSliderDragEnd(int position)
{
    m_bUserIsDraggingSlider = false;

    double targetTime = SliderPosToWorldTime(position);
    PerformSeek(targetTime);

    if (m_bWasPlayingBeforeDrag && m_DemoPlayer)
    {
        m_DemoPlayer->SetPaused(false);
    }

    UpdateTimeCodeLabel(targetTime, false);
}

void CDemoPlayerDialog::OnJumpRelative(double deltaSeconds)
{
    if (!m_DemoPlayer)
        return;

    double curTime = m_DemoPlayer->GetWorldTime();
    double targetTime = curTime + deltaSeconds;
    PerformSeek(targetTime);

    if (m_pTimeSlider && !m_bUserIsDraggingSlider)
    {
        m_pTimeSlider->SetValue(WorldTimeToSliderPos(targetTime), false);
    }
    UpdateTimeCodeLabel(targetTime, false);
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

    // Safety check on dragging state: if mouse button was released outside
    if (m_bUserIsDraggingSlider && m_pTimeSlider && !m_pTimeSlider->IsDragged())
    {
        OnSliderDragEnd(m_pTimeSlider->GetValue());
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

    frame_t *firstFrame = m_World->GetFirstFrame();
    frame_t *lastFrame = m_World->GetLastFrame();

    if (firstFrame && lastFrame)
    {
        frame_t *prevFrame = NULL;
        frame_t *nextFrame = firstFrame;

        while (nextFrame)
        {
            if (prevFrame && (nextFrame->time - prevFrame->time) > 2.0f)
            {
                firstFrame = nextFrame;
                break;
            }
            prevFrame = nextFrame;
            nextFrame = m_World->GetFrameBySeqNr(nextFrame->seqnr + 1);
        }

        m_demoStartTime = firstFrame->time;
        m_demoEndTime = lastFrame->time;
    }

    double worldTime = m_DemoPlayer->GetWorldTime();

    // While user is actively dragging the slider, NEVER overwrite slider value!
    if (!m_bUserIsDraggingSlider && m_pTimeSlider)
    {
        int targetPos = WorldTimeToSliderPos(worldTime);
        if (m_pTimeSlider->GetValue() != targetPos)
        {
            m_pTimeSlider->SetValue(targetPos, false); // bTriggerChangeMessage = false
        }
        UpdateTimeCodeLabel(worldTime, false);
    }

    // Update speed badge button text
    if (m_pButtonSpeedReset)
    {
        float scale = m_DemoPlayer->GetTimeScale();
        char speedBuf[16]{};
        if (std::abs(scale - 1.0f) < 0.05f)
            std::snprintf(speedBuf, sizeof(speedBuf), "1.0x");
        else if (scale < 0.9f)
            std::snprintf(speedBuf, sizeof(speedBuf), "%.2fx", scale);
        else
            std::snprintf(speedBuf, sizeof(speedBuf), "%.1fx", scale);
        m_pButtonSpeedReset->SetText(speedBuf);
    }
}

void CDemoPlayerDialog::OnPlay()
{
    if (!m_DemoPlayer) return;
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

    return true;
}

void CDemoPlayerDialog::OnPause()
{
    if (!m_DemoPlayer) return;
    m_DemoPlayer->SetPaused(true);
    if (m_Engine)
    {
        m_Engine->Cbuf_AddText("stopsound\n");
    }
}

void CDemoPlayerDialog::OnNextFrame(int direction)
{
    if (!m_DemoPlayer || !m_World) return;

    float time = m_DemoPlayer->GetWorldTime();
    frame_t *frame = m_World->GetFrameByTime(time);
    if (!frame) return;

    frame = m_World->GetFrameBySeqNr(frame->seqnr + direction);
    if (!frame) return;

    PerformSeek(frame->time);
    m_DemoPlayer->SetPaused(true);
}

void CDemoPlayerDialog::OnStart()
{
    if (!m_DemoPlayer || !m_World) return;
    frame_t *first = m_World->GetFirstFrame();
    if (!first) return;

    PerformSeek(first->time);
    m_DemoPlayer->SetTimeScale(1.0f);
    m_DemoPlayer->SetPaused(true);
}

void CDemoPlayerDialog::OnEnd()
{
    if (!m_DemoPlayer || !m_World) return;
    frame_t *last = m_World->GetLastFrame();
    if (!last) return;

    PerformSeek(last->time);
    m_DemoPlayer->SetPaused(true);
}

void CDemoPlayerDialog::OnSlower()
{
    if (!m_DemoPlayer) return;
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
    else if (!strcmp(command, "jumpfwd5"))
    {
        OnJumpRelative(+5.0);
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
