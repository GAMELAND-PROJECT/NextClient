#include "FavoriteGames.h"

#include "GameUi.h"
#include "ServerContextMenu.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"
#include "DialogAddServer.h"
#include "FileSystem.h"
#include "utlbuffer.h"

#include <vgui/ISchemeNext.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/MessageBox.h>

using namespace vgui2;

CFavoriteGames::CFavoriteGames(vgui2::Panel *parent) :
    CBaseGamesPage(parent, "FavoriteGames")
{

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

