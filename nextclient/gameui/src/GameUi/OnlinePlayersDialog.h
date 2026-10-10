#pragma once

#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include "PresenceClient.h"
#include <mutex>
#include <vector>

class COnlinePlayersDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(COnlinePlayersDialog, vgui2::Frame);

public:
    COnlinePlayersDialog(vgui2::Panel *parent);
    virtual ~COnlinePlayersDialog();

    virtual void Activate() override;
    virtual void OnKeyCodePressed(vgui2::KeyCode code) override;
    virtual void OnThink() override;

protected:
    virtual void OnCommand(const char *command) override;
    virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
    void RefreshPlayerList();
    void JoinSelectedPlayerServer();
    void PopulateList(const std::vector<PlayerPresenceInfo>& players);

    vgui2::ListPanel *m_pPlayerList;
    vgui2::Label *m_pStatusLabel;
    vgui2::Button *m_pJoinButton;
    vgui2::Button *m_pChatButton;
    vgui2::Button *m_pRefreshButton;
    vgui2::Button *m_pCloseButton;

    std::vector<PlayerPresenceInfo> m_players;

    std::mutex m_listMutex;
    bool m_hasPendingUpdate = false;
    bool m_pendingSuccess = false;
    std::vector<PlayerPresenceInfo> m_pendingPlayers;
};
