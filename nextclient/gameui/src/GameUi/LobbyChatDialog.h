#pragma once

#include <vgui_controls/Frame.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include "PresenceClient.h"
#include <mutex>
#include <vector>
#include <string>

class CLobbyChatDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CLobbyChatDialog, vgui2::Frame);

public:
    CLobbyChatDialog(vgui2::Panel *parent);
    virtual ~CLobbyChatDialog();

    virtual void Activate() override;
    virtual void OnKeyCodePressed(vgui2::KeyCode code) override;
    virtual void OnThink() override;
    virtual void OnMessage(const KeyValues *params, vgui2::VPANEL fromPanel) override;

protected:
    virtual void OnCommand(const char *command) override;
    virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
    void SendChatMessage();
    void PollNewMessages();
    void AppendMessageToHistory(const LobbyChatMessage& msg);

    vgui2::RichText *m_pChatHistory;
    vgui2::TextEntry *m_pMessageInput;
    vgui2::Label *m_pStatusLabel;
    vgui2::Button *m_pSendButton;
    vgui2::Button *m_pCloseButton;

    int m_lastMessageId = 0;
    float m_lastPollTime = 0.0f;
    float m_lastSendTime = 0.0f;
    bool m_isPolling = false;
    bool m_isSending = false;

    struct SendResult
    {
        bool hasResult = false;
        bool success = false;
        std::string errorOrNotice;
        LobbyChatMessage sentMsg;
    };

    std::mutex m_chatMutex;
    std::vector<LobbyChatMessage> m_incomingQueue;
    SendResult m_pendingSendResult;
};
