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

protected:
    virtual void OnCommand(const char *command);
    virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
    virtual void Activate();

private:
    void RefreshDemoList();
    void UploadSelectedDemo();

    vgui2::ListPanel *m_pDemoList;
    vgui2::Button *m_pUploadButton;
    vgui2::Button *m_pRefreshButton;
    vgui2::Button *m_pCloseButton;
};

#endif // DEMOUPLOADERDIALOG_H
