#include "OnlinePlayersDialog.h"
#include "GameUi.h"
#include "BasePanel.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/IScheme.h>
#include <KeyValues.h>

COnlinePlayersDialog::COnlinePlayersDialog(vgui2::Panel *parent)
    : Frame(parent, "OnlinePlayersDialog")
{
    SetBounds(0, 0, 640, 460);
    SetSizeable(false);
    SetTitle("GameLand Live Online Players", true);

    m_pPlayerList = new vgui2::ListPanel(this, "PlayerList");
    m_pPlayerList->AddColumnHeader(0, "name", "Player Name", 160);
    m_pPlayerList->AddColumnHeader(1, "status", "Status", 90);
    m_pPlayerList->AddColumnHeader(2, "map", "Current Map", 120);
    m_pPlayerList->AddColumnHeader(3, "server", "Server / Location", 150);
    m_pPlayerList->AddColumnHeader(4, "edition", "Client", 70);

    m_pStatusLabel = new vgui2::Label(this, "StatusLabel", "Connecting to presence service...");
    m_pJoinButton = new vgui2::Button(this, "JoinButton", "Join Game");
    m_pChatButton = new vgui2::Button(this, "ChatButton", "Global Chat");
    m_pRefreshButton = new vgui2::Button(this, "RefreshButton", "Refresh");
    m_pCloseButton = new vgui2::Button(this, "CloseButton", "Close");

    m_pJoinButton->SetCommand("JoinServer");
    m_pChatButton->SetCommand("OpenChat");
    m_pRefreshButton->SetCommand("Refresh");
    m_pCloseButton->SetCommand("Close");
}

COnlinePlayersDialog::~COnlinePlayersDialog()
{
}

void COnlinePlayersDialog::Activate()
{
    BaseClass::Activate();
    RefreshPlayerList();
}

void COnlinePlayersDialog::OnKeyCodePressed(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_ESCAPE || code == vgui2::KEY_F4)
    {
        OnClose();
        return;
    }
    else if (code == vgui2::KEY_F5)
    {
        RefreshPlayerList();
        return;
    }
    BaseClass::OnKeyCodePressed(code);
}

void COnlinePlayersDialog::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    int screenW = 1024, screenH = 768;
    if (vgui2::surface() != nullptr)
        vgui2::surface()->GetScreenSize(screenW, screenH);
    SetPos((screenW - 640) / 2, (screenH - 460) / 2);

    m_pPlayerList->SetBounds(20, 42, 600, 360);

    int btnY = 414;
    int btnH = 26;
    m_pStatusLabel->SetBounds(20, btnY, 240, btnH);

    m_pJoinButton->SetBounds(270, btnY, 95, btnH);
    m_pChatButton->SetBounds(372, btnY, 95, btnH);
    m_pRefreshButton->SetBounds(474, btnY, 70, btnH);
    m_pCloseButton->SetBounds(550, btnY, 70, btnH);
}

void COnlinePlayersDialog::RefreshPlayerList()
{
    m_pStatusLabel->SetText("Fetching online players...");

    PresenceClient::GetInstance().FetchPlayersListAsync([this](bool success, const std::vector<PlayerPresenceInfo>& players) {
        std::lock_guard<std::mutex> lock(m_listMutex);
        m_hasPendingUpdate = true;
        m_pendingSuccess = success;
        m_pendingPlayers = players;
    });
}

void COnlinePlayersDialog::PopulateList(const std::vector<PlayerPresenceInfo>& players)
{
    m_players = players;
    m_pPlayerList->DeleteAllItems();

    int inLobby = 0;
    int inGame = 0;

    for (size_t i = 0; i < m_players.size(); ++i)
    {
        const auto& p = m_players[i];
        bool isIngame = (p.state == "ingame");
        if (isIngame) inGame++; else inLobby++;

        KeyValues *item = new KeyValues("Item");
        item->SetString("name", p.name.c_str());
        item->SetString("status", isIngame ? "In-Game" : "In-Lobby");
        item->SetString("map", p.map.empty() ? "-" : p.map.c_str());
        item->SetString("server", p.server.empty() ? (isIngame ? "Server" : "Main Menu") : p.server.c_str());

        std::string editionTag = p.tag.empty() ? p.edition : p.tag;
        if (p.edition == "home") editionTag = "HOME";
        item->SetString("edition", editionTag.c_str());
        item->SetInt("index", static_cast<int>(i));

        m_pPlayerList->AddItem(item, 0, false, false);
    }

    char statusBuf[128];
    sprintf_s(statusBuf, "Total: %d  (Lobby: %d | In-Game: %d)", (int)m_players.size(), inLobby, inGame);
    m_pStatusLabel->SetText(statusBuf);
}

void COnlinePlayersDialog::OnThink()
{
    BaseClass::OnThink();

    if (!IsVisible())
        return;

    bool hasUpdate = false;
    bool success = false;
    std::vector<PlayerPresenceInfo> players;

    {
        std::lock_guard<std::mutex> lock(m_listMutex);
        if (m_hasPendingUpdate)
        {
            hasUpdate = true;
            success = m_pendingSuccess;
            players = std::move(m_pendingPlayers);
            m_hasPendingUpdate = false;
        }
    }

    if (hasUpdate)
    {
        if (success)
        {
            PopulateList(players);
        }
        else
        {
            m_pStatusLabel->SetText("Could not connect to community host.");
        }
    }
}

void COnlinePlayersDialog::OnCommand(const char *command)
{
    if (!stricmp(command, "Refresh"))
    {
        RefreshPlayerList();
    }
    else if (!stricmp(command, "JoinServer"))
    {
        JoinSelectedPlayerServer();
    }
    else if (!stricmp(command, "OpenChat"))
    {
        if (BasePanel())
            BasePanel()->OnOpenLobbyChatDialog();
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

void COnlinePlayersDialog::JoinSelectedPlayerServer()
{
    if (m_pPlayerList->GetSelectedItemsCount() <= 0)
        return;

    int itemID = m_pPlayerList->GetSelectedItem(0);
    KeyValues *pItem = m_pPlayerList->GetItem(itemID);
    if (!pItem) return;

    int idx = pItem->GetInt("index", -1);
    if (idx >= 0 && idx < (int)m_players.size())
    {
        const auto& p = m_players[idx];
        if (p.state == "ingame" && !p.server.empty() && p.server != "Server" && p.server != "Main Menu")
        {
            std::string cmd = "connect " + p.server + "\n";
            if (engine)
            {
                OnClose();
                engine->pfnClientCmd(const_cast<char*>(cmd.c_str()));
            }
        }
        else
        {
            m_pStatusLabel->SetText("Player is not in a joinable server.");
        }
    }
}
