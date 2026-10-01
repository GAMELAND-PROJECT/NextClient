//========= Copyright Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerDialog.h: interface for the CDemoPlayerDialog class.
//
//=============================================================================

#if !defined DEMOPLAYERDIALOG_H
#define DEMOPLAYERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <chrono>
#include <vgui_controls/Frame.h>
#include <IBaseSystem.h>
#include <common/BitBuffer.h>
#include <IDemoPlayer.h>
#include <ISystemModule.h>

class IWorld;
class IEngineWrapper;

class CDemoPlayerDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE( CDemoPlayerDialog, Frame );

public:
    CDemoPlayerDialog(vgui2::Panel *parent);
    virtual ~CDemoPlayerDialog();

protected:
    // virtual overrides
    void OnThink() override;
    void OnClose() override;
    virtual void OnCommand(const char *command);

    virtual void ReceiveSignal(ISystemModule * module, unsigned int signal);

    typedef vgui2::Frame BaseClass;

    void Update();       // updates all visible data fields
    bool LoadModules();  // get other modules (world, demo player etc)

    // main panel button events
    void OnPause();     // pause playback
    void OnPlay();      // resume playback
    void OnStart();     // go to start
    void OnEnd();       // go to end
    void OnSlower();    // slower playback speed
    void OnFaster();    // faster playback speed
    void OnResetSpeed(); // reset speed to 1.0x
    void OnLoad();      // load new demo dialog
    void OnStop();      // stop demo playback completely
    void OnNextFrame(int direction); // next/last frame
    void OnEvents();    // open demo events editor
    void OnSave();      // save demo file again
    void OnJumpRelative(double deltaSeconds); // jump +- delta seconds

    // Slider action message handlers
    MESSAGE_FUNC_INT( OnSliderMoved, "SliderMoved", position );
    MESSAGE_FUNC_INT( OnSliderDragStart, "SliderDragStart", position );
    MESSAGE_FUNC_INT( OnSliderDragEnd, "SliderDragEnd", position );

    // High precision seek & conversion helpers
    double SliderPosToWorldTime(int pos);
    int WorldTimeToSliderPos(double worldTime);
    void PerformSeek(double targetTime);
    void UpdateTimeCodeLabel(double worldTime, bool isSeeking);

public:
    MESSAGE_FUNC_CHARPTR(DemoSelected, "DemoSelected", demoname); // select demo

private:
    MESSAGE_FUNC_INT( ButtonToggled, "ButtonToggled", state);

protected:
    void ApplySchemeSettings( vgui2::IScheme *pScheme ) override;

    vgui2::Label            *m_pLableTimeCode;
    vgui2::Slider           *m_pTimeSlider;
    vgui2::ToggleButton     *m_MasterButton;

    vgui2::Button           *m_pButtonPlay;
    vgui2::Button           *m_pButtonStepF;        // |>
    vgui2::Button           *m_pButtonFaster;       // >>
    vgui2::Button           *m_pButtonSlower;       // <<
    vgui2::Button           *m_pButtonStepB;        // <|
    vgui2::Button           *m_pButtonPause;        // ||
    vgui2::Button           *m_pButtonStart;        // |<
    vgui2::Button           *m_pButtonEnd;          // >|
    vgui2::Button           *m_pButtonLoad;         // ^
    vgui2::Button           *m_pButtonStop;         // x

    // Enhanced jump & speed controls
    vgui2::Button           *m_pButtonJumpBack;     // -5s
    vgui2::Button           *m_pButtonJumpFwd;      // +5s
    vgui2::Button           *m_pButtonSpeedReset;   // 1.0x

    vgui2::DHANDLE<vgui2::Frame>    m_hDemoPlayerFileDialog;
    vgui2::DHANDLE<vgui2::Frame>    m_hDemoEventsDialog;

    IEngineWrapper          *m_Engine;
    IDemoPlayer             *m_DemoPlayer;
    IBaseSystem             *m_System;
    IWorld                  *m_World;

    float                   m_NextTimeScale;
    int                     m_lastSliderTime;

    // Robust scrub state tracking
    bool                    m_bUserIsDraggingSlider{false};
    bool                    m_bWasPlayingBeforeDrag{false};
    std::chrono::steady_clock::time_point m_lastSeekTime{};
    double                  m_demoStartTime{0.0};
    double                  m_demoEndTime{0.0};
};

#endif // !defined DEMOPLAYERDIALOG_H
