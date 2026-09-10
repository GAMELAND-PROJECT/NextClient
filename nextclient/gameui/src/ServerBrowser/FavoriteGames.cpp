#include "FavoriteGames.h"

#include "GameUi.h"
#include "ServerContextMenu.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"
#include "DialogAddServer.h"
#include "FileSystem.h"
#include "OnlineServerPresentation.h"
#include "utlbuffer.h"

#include <vgui/ISchemeNext.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Label.h>
#include <GameServerHelpers.h>
#include <algorithm>
#include <unordered_set>

using namespace vgui2;

namespace
{
int __cdecl PopulationCompare(ListPanel*, const ListPanelItem& a, const ListPanelItem& b)
{
    const int playersA = a.kv->GetInt("_humans", -1), playersB = b.kv->GetInt("_humans", -1);
    const int pingA = a.kv->GetInt("_latency", 9999), pingB = b.kv->GetInt("_latency", 9999);
    return CompareOnlinePopulation(playersA, pingA, a.kv->GetString("_endpoint"),
        playersB, pingB, b.kv->GetString("_endpoint"));
}

class COnlineCategoryList : public CGameListPanel
{
public:
    COnlineCategoryList(CFavoriteGames* owner, const char* name) : CGameListPanel(owner, name), owner_(owner)
    {
        AddActionSignalTarget(owner);
        AddColumnHeader(0, "Name", "Server", 180, 80, 4000, COLUMN_RESIZEWITHWINDOW | COLUMN_UNHIDABLE);
        AddColumnHeader(1, "Players", "Players", 62, COLUMN_FIXEDSIZE | COLUMN_UNHIDABLE);
        AddColumnHeader(2, "Ping", "Ping", 45, COLUMN_FIXEDSIZE | COLUMN_UNHIDABLE);
        for (int column = 0; column < 3; ++column) SetColumnSortable(column, false);
        SetPinnedSortFunc(1, PopulationCompare);
        SetSortColumnEx(1, -1, true);
        SetMultiselectEnabled(false);
        SetAllowUserModificationOfColumns(false);
        SetColumnTextAlignment(1, Label::a_center);
        SetColumnTextAlignment(2, Label::a_center);
        SetEmptyListText("No servers in this category");
    }
    void OnMousePressed(MouseCode code) override
    {
        CGameListPanel::OnMousePressed(code);
        owner_->SelectOnlineServer(this);
    }
    void OnKeyCodeTyped(KeyCode code) override
    {
        owner_->SelectOnlineServer(this);
        CGameListPanel::OnKeyCodeTyped(code);
        owner_->SelectOnlineServer(this);
    }
private:
    CFavoriteGames* owner_;
};
}

CFavoriteGames::CFavoriteGames(vgui2::Panel *parent) :
    CBaseGamesPage(parent, "FavoriteGames", nullptr,
        {GameListColumnType::ServerName, GameListColumnType::Players, GameListColumnType::Ping})
{
    m_publicList = new COnlineCategoryList(this, "OnlinePublicList");
    m_mixList = new COnlineCategoryList(this, "OnlineMixList");
    m_publicHeading = new Label(this, "PublicHeading", "PUBLIC");
    m_mixHeading = new Label(this, "MixHeading", "MIX");
    m_separator = new Panel(this, "OnlineDivider");
    m_separator->SetMouseInputEnabled(false);
    m_pGameList->SetVisible(false);
}

void CFavoriteGames::PerformLayout()
{
    BaseClass::PerformLayout();
    if (!m_publicList) return;
    m_pGameList->SetVisible(false);
    int x, y, width, height;
    m_pGameList->GetBounds(x, y, width, height);
    const int gap = 16, heading = 32, half = std::max(1, (width - gap) / 2);
    m_publicHeading->SetBounds(x + 10, y, half - 10, heading);
    m_mixHeading->SetBounds(x + half + gap + 10, y, width - half - gap - 10, heading);
    m_publicList->SetBounds(x, y + heading, half, std::max(1, height - heading));
    m_mixList->SetBounds(x + half + gap, y + heading, width - half - gap, std::max(1, height - heading));
    m_separator->SetBounds(x + half + gap / 2, y + 8, 1, std::max(1, height - 8));
}

void CFavoriteGames::ApplySchemeSettings(IScheme* scheme)
{
    BaseClass::ApplySchemeSettings(scheme);
    if (!m_publicList) return;
    for (auto* list : {m_publicList, m_mixList})
    {
        list->SetBgColor(Color(20, 25, 31, 255));
        list->SetFgColor(Color(229, 234, 240, 255));
    }
    m_publicHeading->SetFgColor(Color(101, 215, 185, 255));
    m_mixHeading->SetFgColor(Color(115, 177, 249, 255));
    m_publicHeading->SetFont(scheme->GetFont("DefaultBold", IsProportional()));
    m_mixHeading->SetFont(scheme->GetFont("DefaultBold", IsProportional()));
    m_separator->SetBgColor(Color(58, 70, 84, 255));
}

void CFavoriteGames::SelectOnlineServer(CGameListPanel* source)
{
    if (!source->GetSelectedItemsCount()) return;
    const int serverID = source->GetItemUserData(source->GetSelectedItem(0));
    if (!m_Servers.IsServerExists(serverID)) return;
    const int backingID = m_Servers.GetServer(serverID).listEntryID;
    if (!m_pGameList->IsValidItemID(backingID)) return;
    (source == m_publicList ? m_mixList : m_publicList)->ClearSelectedItems();
    m_pGameList->SetSingleSelectedItem(backingID);
    m_pConnect->SetEnabled(true);
}

void CFavoriteGames::OnThink()
{
    BaseClass::OnThink();
    if (IsVisible() && m_categoriesDirty)
    {
        m_categoriesDirty = false;
        UpdateCategoryLists();
    }
}

void CFavoriteGames::ServerResponded(serveritem_t& server)
{
    BaseClass::ServerResponded(server);
    m_categoriesDirty = true;
}

void CFavoriteGames::ApplyFilters()
{
    BaseClass::ApplyFilters();
    m_categoriesDirty = true;
}

void CFavoriteGames::UpdateCategoryLists()
{
    std::unordered_set<int> present;
    for (int row = 0; row < m_pGameList->GetItemCount(); ++row)
    {
        const int backingID = m_pGameList->GetItemIDFromRow(row);
        const int serverID = m_pGameList->GetItemUserData(backingID);
        if (!m_Servers.IsServerExists(serverID)) continue;
        const auto& server = m_Servers.GetServer(serverID).gs;
        const auto endpoint = server.m_NetAdr.GetConnectionAddressString();
        const bool mix = server.m_ulTimeLastPlayed == kOnlineMixServerMarker;
        auto* list = mix ? m_mixList : m_publicList;
        present.insert(serverID);
        auto existing = m_categoryRows.find(serverID);
        if (existing != m_categoryRows.end() && existing->second.mix != mix)
        {
            (existing->second.mix ? m_mixList : m_publicList)->RemoveItem(existing->second.item);
            m_categoryRows.erase(existing);
            existing = m_categoryRows.end();
        }
        const bool fresh = existing == m_categoryRows.end();
        KeyValues* data = fresh ? new KeyValues("Server") : list->GetItem(existing->second.item);
        data->SetString("Name", server.GetName().c_str());
        data->SetString("_endpoint", endpoint.c_str());
        data->SetInt("_humans", server.m_bHadSuccessfulResponse ? GetHumanPlayerCount(server) : -1);
        data->SetInt("_latency", server.m_bHadSuccessfulResponse ? server.m_nPing : 9999);
        if (server.m_bHadSuccessfulResponse)
        {
            char count[32];
            Q_snprintf(count, sizeof(count), "%d / %d", GetHumanPlayerCount(server), server.m_nMaxPlayers);
            data->SetString("Players", count);
            data->SetInt("Ping", server.m_nPing);
        }
        else
        {
            data->SetString("Players", "-");
            data->SetString("Ping", "-");
        }
        if (fresh)
        {
            const int id = list->AddItem(data, serverID, false, false);
            data->deleteThis();
            m_categoryRows.emplace(serverID, CategoryRow{mix, id});
        }
        else list->ApplyItemChanges(existing->second.item);
    }
    for (auto it = m_categoryRows.begin(); it != m_categoryRows.end();)
    {
        if (!present.contains(it->first))
        {
            (it->second.mix ? m_mixList : m_publicList)->RemoveItem(it->second.item);
            it = m_categoryRows.erase(it);
        }
        else ++it;
    }
    m_publicList->SortList();
    m_mixList->SortList();
    if (m_pGameList->GetSelectedItemsCount())
    {
        const int selected = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));
        if (const auto found = m_categoryRows.find(selected); found != m_categoryRows.end())
        {
            auto* list = found->second.mix ? m_mixList : m_publicList;
            if (!list->IsItemSelected(found->second.item)) list->SetSingleSelectedItem(found->second.item);
        }
        else
        {
            m_pGameList->ClearSelectedItems();
            m_pConnect->SetEnabled(false);
        }
    }
}

CFavoriteGames::~CFavoriteGames()
{
}

void CFavoriteGames::OnPageShow()
{
    if (!ServerBrowserDialog().IsVisible())
        return;

    if (m_ColumnsMap.contains(GameListColumnType::Players))
        m_pGameList->SetSortColumnEx(m_ColumnsMap[GameListColumnType::Players], -1, true);

    // Match the known-good f5addc2 lifecycle: every page activation creates a
    // fresh Favorites request instead of reusing a cancelled/stale snapshot.
    GetNewServerList();
}

void CFavoriteGames::OnPageHide()
{
    StopRefresh(CancelQueryReason::PageClosed);
}

void CFavoriteGames::OnViewGameInfo()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

bool CFavoriteGames::SupportsItem(InterfaceItem item)
{
    switch (item)
    {
        case InterfaceItem::Filters: return true;
    }

    return false;
}

void CFavoriteGames::StartRefresh()
{
    StopRefresh(CancelQueryReason::NewQuery);

    // A quick refresh only works while the page still owns a valid request.
    // Rebuild the list after first open, cancellation, or request release.
    if (!m_Servers.StartRefresh())
    {
        GetNewServerList();
        return;
    }

    SetRefreshing(true);
}

void CFavoriteGames::GetNewServerList()
{
    if (!IsVisible())
        return;

    StopRefresh(CancelQueryReason::NewQuery);

    m_publicList->DeleteAllItems();
    m_mixList->DeleteAllItems();
    m_categoryRows.clear();

    m_pGameList->DeleteAllItems();

    m_Servers.Clear();
    m_Servers.RequestFavorites(GetFilter(), GetFilterCount());

    UpdateRefreshStatusText();
    SetRefreshing(true);
}

void CFavoriteGames::StopRefresh(CancelQueryReason reason)
{
    CBaseGamesPage::StopRefresh(reason);
}

GuiConnectionSource CFavoriteGames::GetConnectionSource()
{
    return GuiConnectionSource::ServersFavorites;
}

serveritem_t &CFavoriteGames::GetServer(int serverID)
{
    return CBaseGamesPage::GetServer(serverID);
}

void CFavoriteGames::AddNewServer(uint32_t ip, uint16_t port)
{
    // Preserve this legacy entry point as a guarded no-op. Only the managed
    // pinned-server source may populate the Online/Favorites page.
    (void)ip;
    (void)port;
}

void CFavoriteGames::OnAddCurrentServer()
{
    // Intentionally disabled for the managed Online list.
}

void CFavoriteGames::RefreshComplete()
{
    m_categoriesDirty = true;
    SetRefreshing(false);
    UpdateFilterSettings();
    UpdateRefreshStatusText();

    if (IsVisible())
        m_pGameList->SortList();
}

void CFavoriteGames::ServerFailedToRespond(serveritem_t &server)
{
    ServerResponded(server);
}

void CFavoriteGames::OnOpenContextMenu(int itemID)
{
    CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(m_pGameList);

    if (m_pGameList->GetSelectedItemsCount())
    {
        int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

        menu->ShowMenu(this, serverID, true, true, true, false);
    }
    else
        menu->ShowMenu(this, (unsigned int)-1, false, false, false, false);
}

void CFavoriteGames::OnRefreshServer(int serverID)
{
    if (m_Servers.IsRefreshing())
        return;

    m_Servers.StartRefreshServer(serverID);
}

void CFavoriteGames::OnRemoveFromFavorites()
{
    // Managed pins cannot be removed locally.
}

void CFavoriteGames::OnAddServerByName()
{
    // Intentionally disabled for the managed Online list.
}

void CFavoriteGames::OnCommand(const char *command)
{
    if (!Q_stricmp(command, "AddServerByName") ||
        !Q_stricmp(command, "AddCurrentServer"))
        return;

    BaseClass::OnCommand(command);
}

