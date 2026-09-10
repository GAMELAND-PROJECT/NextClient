#ifndef FAVORITEGAMES_H
#define FAVORITEGAMES_H

#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

#include "IGameList.h"
#include "serveritem.h"
#include <unordered_map>

namespace vgui2 { class Label; }

class CFavoriteGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CFavoriteGames, CBaseGamesPage);

    void AddNewServer(uint32_t ip, uint16_t port);

public:
    explicit CFavoriteGames(vgui2::Panel *parent);
    ~CFavoriteGames() override;

    void OnPageShow() override;
    void OnPageHide() override;
    void OnViewGameInfo() override;
    void PerformLayout() override;
    void OnThink() override;
    void ApplySchemeSettings(vgui2::IScheme* scheme) override;
    void SelectOnlineServer(CGameListPanel* source);

    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;
    void GetNewServerList() override;
    void StopRefresh(CancelQueryReason reason) override;

    GuiConnectionSource GetConnectionSource() override;
    serveritem_t &GetServer(int serverID) override;

protected:
    // IServerRefreshResponse
    void RefreshComplete() override;
    void ServerFailedToRespond(serveritem_t &server) override;
    void ServerResponded(serveritem_t &server) override;
    void ApplyFilters() override;

private:
    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);
    MESSAGE_FUNC(OnRemoveFromFavorites, "RemoveFromFavorites");
    MESSAGE_FUNC(OnAddServerByName, "AddServerByName");

private:
    void UpdateCategoryLists();
    CGameListPanel* m_publicList{};
    CGameListPanel* m_mixList{};
    vgui2::Label* m_publicHeading{};
    vgui2::Label* m_mixHeading{};
    vgui2::Panel* m_separator{};
    struct CategoryRow { bool mix; int item; };
    std::unordered_map<int, CategoryRow> m_categoryRows;
    bool m_categoriesDirty = true;
    void OnRefreshServer(int serverID);
    void OnAddCurrentServer();
    void OnCommand(const char *command);

};

#endif
