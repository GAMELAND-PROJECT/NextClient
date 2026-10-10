#include "LobbyChatDialog.h"
#include "GameUi.h"
#include "BasePanel.h"
#include "PersianShaper.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>

CLobbyChatDialog::CLobbyChatDialog(vgui2::Panel *parent)
    : Frame(parent, "LobbyChatDialog")
{
    SetBounds(0, 0, 560, 420);
    SetSizeable(false);
    SetTitle("GameLand Global Lobby Chat", true);

    m_pChatHistory = new vgui2::RichText(this, "ChatHistory");
    m_pChatHistory->SetVerticalScrollbar(true);
    m_pChatHistory->SetMaximumCharCount(50000);

    m_pMessageInput = new vgui2::TextEntry(this, "MessageInput");
    m_pMessageInput->SetMaximumCharCount(180);
    m_pMessageInput->SetAllowNonAsciiCharacters(true);
    m_pMessageInput->SendNewLine(true);
    m_pMessageInput->SetCatchEnterKey(true);

    m_pStatusLabel = new vgui2::Label(this, "StatusLabel", "Press Enter to chat. Keep messages friendly.");
    m_pSendButton = new vgui2::Button(this, "SendButton", "Send (Enter)");
    m_pCloseButton = new vgui2::Button(this, "CloseButton", "Close (ESC)");

    m_pSendButton->SetCommand("Send");
    m_pCloseButton->SetCommand("Close");

    // Welcome banner in chat
    m_pChatHistory->InsertColorChange(Color(0, 220, 255, 255));
    m_pChatHistory->InsertString("[SYSTEM] Welcome to GameLand Global Lobby Chat!\n");
    m_pChatHistory->InsertColorChange(Color(170, 185, 200, 255));
    m_pChatHistory->InsertString("[SYSTEM] Chat with players in lobby, organize matches and find teammates.\n\n");
}

CLobbyChatDialog::~CLobbyChatDialog()
{
}

void CLobbyChatDialog::Activate()
{
    BaseClass::Activate();
    m_pMessageInput->RequestFocus();
    m_lastPollTime = 0.0f;
    PollNewMessages();
}

void CLobbyChatDialog::OnKeyCodePressed(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_ENTER)
    {
        SendChatMessage();
        return;
    }
    else if (code == vgui2::KEY_ESCAPE || code == vgui2::KEY_F4)
    {
        OnClose();
        return;
    }
    BaseClass::OnKeyCodePressed(code);
}

void CLobbyChatDialog::OnMessage(const KeyValues *params, vgui2::VPANEL fromPanel)
{
    if (params)
    {
        const char *name = params->GetName();
        if (name && !stricmp(name, "TextNewLine"))
        {
            SendChatMessage();
            return;
        }
    }
    BaseClass::OnMessage(params, fromPanel);
}

void CLobbyChatDialog::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    int screenW = 1024, screenH = 768;
    if (vgui2::surface() != nullptr)
        vgui2::surface()->GetScreenSize(screenW, screenH);
    SetPos((screenW - 560) / 2, (screenH - 420) / 2);

    m_pChatHistory->SetBounds(20, 42, 520, 305);
    m_pChatHistory->SetBgColor(Color(20, 24, 30, 230));

    m_pMessageInput->SetBounds(20, 354, 420, 26);
    m_pMessageInput->SetBgColor(Color(25, 30, 38, 255));
    m_pMessageInput->SetFgColor(Color(240, 245, 255, 255));

    m_pSendButton->SetBounds(445, 354, 95, 26);

    m_pStatusLabel->SetBounds(20, 386, 420, 22);
    m_pCloseButton->SetBounds(445, 386, 95, 24);
}

void CLobbyChatDialog::OnThink()
{
    BaseClass::OnThink();

    if (!IsVisible())
        return;

    // 1. Process send results from background worker thread
    bool hadSendResult = false;
    SendResult sendRes;
    std::vector<LobbyChatMessage> newMessages;

    {
        std::lock_guard<std::mutex> lock(m_chatMutex);
        if (m_pendingSendResult.hasResult)
        {
            hadSendResult = true;
            sendRes = m_pendingSendResult;
            m_pendingSendResult.hasResult = false;
        }
        if (!m_incomingQueue.empty())
        {
            newMessages = std::move(m_incomingQueue);
            m_incomingQueue.clear();
        }
    }

    if (hadSendResult)
    {
        m_isSending = false;
        if (sendRes.success)
        {
            m_pStatusLabel->SetText("Message sent. Keep messages friendly.");
            m_pMessageInput->SetText("");

            // Immediately show the sent message if it has a valid ID
            if (sendRes.sentMsg.id > 0 && sendRes.sentMsg.id > m_lastMessageId)
            {
                AppendMessageToHistory(sendRes.sentMsg);
                m_lastMessageId = sendRes.sentMsg.id;
            }

            // Immediately poll to fetch any other messages
            PollNewMessages();
        }
        else
        {
            std::string err = "Failed: ";
            err += sendRes.errorOrNotice.empty() ? "Network error" : sendRes.errorOrNotice;
            m_pStatusLabel->SetText(err.c_str());
        }
        m_pMessageInput->RequestFocus();
    }

    // 2. Process incoming chat messages
    for (const auto& m : newMessages)
    {
        if (m.id > m_lastMessageId)
        {
            AppendMessageToHistory(m);
            m_lastMessageId = m.id;
        }
    }

    // 3. Periodic polling every 2.0 seconds while dialog is open
    float curTime = vgui2::system() ? (float)(vgui2::system()->GetTimeMillis() / 1000.0) : 0.0f;
    if (curTime - m_lastPollTime >= 2.0f && !m_isPolling)
    {
        m_lastPollTime = curTime;
        PollNewMessages();
    }
}

void CLobbyChatDialog::PollNewMessages()
{
    if (m_isPolling) return;
    m_isPolling = true;

    int since = m_lastMessageId;
    PresenceClient::GetInstance().FetchChatMessagesAsync(since, [this](bool success, int lastId, const std::vector<LobbyChatMessage>& messages) {
        m_isPolling = false;
        if (!success || messages.empty())
            return;

        std::lock_guard<std::mutex> lock(m_chatMutex);
        for (const auto& msg : messages)
        {
            m_incomingQueue.push_back(msg);
        }
    });
}

void CLobbyChatDialog::AppendMessageToHistory(const LobbyChatMessage& msg)
{
    // Timestamp [14:32]
    m_pChatHistory->InsertColorChange(Color(130, 140, 155, 255));
    std::string timeStr = "[" + (msg.time.empty() ? "..." : msg.time) + "] ";
    m_pChatHistory->InsertString(timeStr.c_str());

    // Tag badge [FARSHID] or [HOME]
    if (!msg.tag.empty())
    {
        if (msg.edition == "home")
            m_pChatHistory->InsertColorChange(Color(255, 175, 55, 255)); // Esports Gold
        else
            m_pChatHistory->InsertColorChange(Color(0, 210, 160, 255));  // Emerald Mint

        std::string tagStr = "[" + msg.tag + "] ";
        m_pChatHistory->InsertString(tagStr.c_str());
    }

    // Sender Name
    m_pChatHistory->InsertColorChange(Color(220, 235, 255, 255));
    std::wstring shapedSender = Persian::ShapeAndBiDi(Persian::Utf8ToWide(msg.sender));
    std::wstring senderStr = shapedSender + L": ";
    m_pChatHistory->InsertString(senderStr.c_str());

    // Message text
    m_pChatHistory->InsertColorChange(Color(245, 245, 245, 255));
    std::wstring shapedText = Persian::ShapeAndBiDi(Persian::Utf8ToWide(msg.text));
    std::wstring textStr = shapedText + L"\n";
    m_pChatHistory->InsertString(textStr.c_str());

    m_pChatHistory->GotoTextEnd();
}

void CLobbyChatDialog::SendChatMessage()
{
    wchar_t textWBuf[512]{};
    m_pMessageInput->GetText(textWBuf, sizeof(textWBuf));

    std::wstring wtext = textWBuf;
    while (!wtext.empty() && (wtext.front() == L' ' || wtext.front() == L'\t')) wtext.erase(wtext.begin());
    while (!wtext.empty() && (wtext.back() == L' ' || wtext.back() == L'\t' || wtext.back() == L'\r' || wtext.back() == L'\n')) wtext.pop_back();

    if (wtext.empty())
        return;

    std::string text = Persian::WideToUtf8(wtext);

    float curTime = vgui2::system() ? (float)(vgui2::system()->GetTimeMillis() / 1000.0) : 0.0f;
    if (curTime - m_lastSendTime < 3.0f)
    {
        m_pStatusLabel->SetText("Please wait 3 seconds before sending another message.");
        return;
    }

    if (m_isSending)
        return;

    m_isSending = true;
    m_lastSendTime = curTime;
    m_pStatusLabel->SetText("Sending message...");

    PresenceClient::GetInstance().SendChatMessageAsync(text, [this](bool success, const std::string& errorOrNotice, const LobbyChatMessage& sentMsg) {
        std::lock_guard<std::mutex> lock(m_chatMutex);
        m_pendingSendResult.hasResult = true;
        m_pendingSendResult.success = success;
        m_pendingSendResult.errorOrNotice = errorOrNotice;
        m_pendingSendResult.sentMsg = sentMsg;
    });
}

void CLobbyChatDialog::OnCommand(const char *command)
{
    if (!stricmp(command, "Send") || !stricmp(command, "TextNewLine"))
    {
        SendChatMessage();
    }
    else if (!stricmp(command, "Close"))
    {
        OnClose();
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}
