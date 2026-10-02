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
#include <string>
#include <vector>
#include <vgui_controls/Frame.h>
#include <vgui_controls/ProgressBar.h>
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
    void OnSetSpeed(float speed); // direct speed preset (0.5x, 1x, 2x, 4x, 8x, 16x)
    void OnPrevRound(); // jump to previous or current round start
    void OnNextRound(); // fast forward cleanly to next round start
    void OnLoad();      // load new demo dialog
    void OnStop();      // stop demo playback completely
    void OnNextFrame(int direction); // next/last frame
    void OnEvents();    // open demo events editor
    void OnSave();      // save demo file again
    void OnJumpRelative(double deltaSeconds); // jump +- delta seconds (e.g. -5s instant replay)

    // High precision seek & conversion helpers
    void PerformSeek(double targetTime);
    void UpdateTimeCodeLabel(double worldTime, bool isSeeking);
    void ScanRoundBookmarks();

public:
    MESSAGE_FUNC_CHARPTR(DemoSelected, "DemoSelected", demoname); // select demo

private:
    MESSAGE_FUNC_INT( ButtonToggled, "ButtonToggled", state);

protected:
    void ApplySchemeSettings( vgui2::IScheme *pScheme ) override;

    vgui2::Label                    *m_pLableTimeCode;
    vgui2::ContinuousProgressBar    *m_pProgressBar;
    vgui2::ToggleButton             *m_MasterButton;

    vgui2::Button                   *m_pButtonPlay;
    vgui2::Button                   *m_pButtonPause;
    vgui2::Button                   *m_pButtonStepF;        // |>
    vgui2::Button                   *m_pButtonStepB;        // <|
    vgui2::Button                   *m_pButtonStart;        // |<
    vgui2::Button                   *m_pButtonEnd;          // >|
    vgui2::Button                   *m_pButtonLoad;         // ^
    vgui2::Button                   *m_pButtonStop;         // x

    // Round navigation & Instant replay
    vgui2::Button                   *m_pButtonPrevRound;    // |<< Rnd
    vgui2::Button                   *m_pButtonNextRound;    // Rnd >>|
    vgui2::Button                   *m_pButtonJumpBack;     // -5s

    // Multi-speed presets
    vgui2::Button                   *m_pButtonSpeedHalf;    // 0.5x
    vgui2::Button                   *m_pButtonSpeed1x;      // 1.0x
    vgui2::Button                   *m_pButtonSpeed2x;      // 2x
    vgui2::Button                   *m_pButtonSpeed4x;      // 4x
    vgui2::Button                   *m_pButtonSpeed8x;      // 8x
    vgui2::Button                   *m_pButtonSpeed16x;     // 16x

    vgui2::DHANDLE<vgui2::Frame>    m_hDemoPlayerFileDialog;
    vgui2::DHANDLE<vgui2::Frame>    m_hDemoEventsDialog;

    IEngineWrapper                  *m_Engine;
    IDemoPlayer                     *m_DemoPlayer;
    IBaseSystem                     *m_System;
    IWorld                          *m_World;

    float                           m_NextTimeScale;

    // Robust seek state & Round tracking
    bool                            m_bIsSeeking{false};
    double                          m_targetSeekTime{0.0};
    std::chrono::steady_clock::time_point m_seekStartTime{};
    std::string                     m_lastLoadedDemo{};
    double                          m_demoStartTime{0.0};
    double                          m_demoEndTime{0.0};

    std::vector<double>             m_RoundStartTimes;
    bool                            m_bFastForwardingToNextRound{false};
    double                          m_targetRoundTime{0.0};
};

#endif // !defined DEMOPLAYERDIALOG_H
