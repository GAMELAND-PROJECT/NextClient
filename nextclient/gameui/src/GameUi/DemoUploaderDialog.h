#ifndef DEMOUPLOADERDIALOG_H
#define DEMOUPLOADERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/Button.h>

class CDemoUploaderDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CDemoUploaderDialog, vgui2::Frame);

public:
    CDemoUploaderDialog(vgui2::Panel *parent);
    virtual ~CDemoUploaderDialog();
    virtual void Activate();

    void OnKeyCodePressed(vgui2::KeyCode code) override;

protected:
    virtual void OnCommand(const char *command);
    virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

private:
    void RefreshDemoList();
    void PlaySelectedDemo();
    void DeleteSelectedDemo();
    void OpenDemosFolder();

    vgui2::ListPanel *m_pDemoList;
    vgui2::Button *m_pPlayButton;
    vgui2::Button *m_pDeleteButton;
    vgui2::Button *m_pOpenFolderButton;
    vgui2::Button *m_pRefreshButton;
    vgui2::Button *m_pCloseButton;
};

#endif // DEMOUPLOADERDIALOG_H