#include "SteamStubs.h"

SteamFriendsStub g_SteamFriendsStub;
SteamAppsStub g_SteamAppsStub;
SteamGameServerStub g_SteamGameServerStub;
SteamUtilsStub g_SteamUtilsStub;
SteamMatchmakingStub g_SteamMatchmakingStub;
SteamMatchmakingServersStub g_SteamMatchmakingServersStub;
SteamHTTPStub g_SteamHTTPStub;

static gameserveritem_t g_DummyGameServerItem;

gameserveritem_t *SteamMatchmakingServersStub::GetServerDetails(HServerListRequest, int)
{
    return &g_DummyGameServerItem;
}
