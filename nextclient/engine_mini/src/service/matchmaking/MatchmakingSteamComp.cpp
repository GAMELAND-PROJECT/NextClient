#include "MatchmakingSteamComp.h"
#include "engine.h"
#include "master/FileMasterClient.h"
#include "master/HttpMasterClient.h"

#include <algorithm>
#include <cassert>
#include <optick.h>
#include <strtools.h>

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

namespace
{
constexpr char kPinnedServersUrl[] =
    "https://gameland.cam/pinned_servers.txt";
constexpr wchar_t kPinnedServersCacheFile[] = L"pinned_servers.dat";
constexpr size_t kMaxPinnedServers = 64;
}

MatchmakingSteamComp::MatchmakingSteamComp()
{
    source_query_ = std::make_shared<MultiSourceQuery>(750, 3);
    matchmaking_service_ = std::make_shared<MatchmakingService>(source_query_);
}

MatchmakingSteamComp::~MatchmakingSteamComp()
{
    if (pinned_cancellation_token_)
        pinned_cancellation_token_->SetCanceled();

    std::vector<HServerListRequest> request_ids;
    request_ids.reserve(server_requests_.size());
    for (const auto& [request_id, request] : server_requests_)
        request_ids.push_back(request_id);

    for (const auto request_id : request_ids)
        ReleaseRequest(request_id);
}

void MatchmakingSteamComp::InitializePinnedServers()
{
    if (pinned_servers_initialized_)
        return;

    pinned_servers_initialized_ = true;
    pinned_cancellation_token_ = CancellationToken::Create();
    pinned_cache_client_ = std::make_shared<FileMasterClient>(kPinnedServersCacheFile);
    pinned_http_client_ = std::make_shared<HttpMasterClient>(
        g_NextClientVersion,
        kPinnedServersUrl,
        true);

    const auto cancellation_token = pinned_cancellation_token_;
    const auto cache_client = pinned_cache_client_;
    const auto http_client = pinned_http_client_;

    TaskCoro::RunInMainThread([this, cancellation_token, cache_client, http_client]() -> result<void>
    {
        auto cached_addresses = co_await cache_client->GetServerAddressesAsync({}, cancellation_token);
        cancellation_token->ThrowIfCancelled();
        if (cached_addresses.size() > kMaxPinnedServers)
            cached_addresses.resize(kMaxPinnedServers);
        ApplyPinnedServers(cached_addresses);

        auto downloaded_addresses = co_await http_client->GetServerAddressesAsync({}, cancellation_token);
        cancellation_token->ThrowIfCancelled();

        // An empty response is treated as a failed/invalid update. It must not
        // erase a previously working cache or managed server list.
        if (downloaded_addresses.empty())
            co_return;

        if (downloaded_addresses.size() > kMaxPinnedServers)
            downloaded_addresses.resize(kMaxPinnedServers);

        ApplyPinnedServers(downloaded_addresses);
        co_await TaskCoro::RunIO([cache_client, downloaded_addresses]
        {
            cache_client->Save(downloaded_addresses);
        });
    });
}

bool MatchmakingSteamComp::IsPinnedServer(uint32 ip, uint16 port) const
{
    return pinned_servers_.contains(MakePinnedServerKey(ip, port));
}

void MatchmakingSteamComp::ApplyPinnedServers(const std::vector<netadr_t>& addresses)
{
    std::unordered_set<uint64_t> updated_servers;
    updated_servers.reserve(addresses.size());

    for (const auto& address : addresses)
    {
        if (!address.IsValid())
            continue;

        const auto ip = address.GetIPHostByteOrder();
        const auto port = address.GetPortHostByteOrder();
        updated_servers.emplace(MakePinnedServerKey(ip, port));
    }

    pinned_servers_ = std::move(updated_servers);
    RestartFavoriteRequests();
}

std::vector<gameserveritem_t> MatchmakingSteamComp::BuildFavoriteServerList()
{
    std::vector<gameserveritem_t> servers;
    std::unordered_set<uint64_t> endpoints;

    const auto app_id = SteamUtils()->GetAppID();
    const int favorite_count = SteamMatchmaking()->GetFavoriteGameCount();
    servers.reserve(static_cast<size_t>(std::max(favorite_count, 0)) + pinned_servers_.size());

    for (int i = 0; i < favorite_count; ++i)
    {
        AppId_t favorite_app_id{};
        uint32 ip{};
        uint16 connection_port{};
        uint16 query_port{};
        uint32 flags{};
        uint32 last_played{};
        if (!SteamMatchmaking()->GetFavoriteGame(
                i, &favorite_app_id, &ip, &connection_port, &query_port, &flags, &last_played) ||
            favorite_app_id != app_id || !(flags & k_unFavoriteFlagFavorite))
        {
            continue;
        }

        if (!endpoints.emplace(MakePinnedServerKey(ip, connection_port)).second)
            continue;

        gameserveritem_t server{};
        InitEmptyGameServerItem(server, ip, connection_port);
        server.m_NetAdr.Init(ip, query_port, connection_port);
        server.m_ulTimeLastPlayed = last_played;
        servers.push_back(server);
    }

    // Managed pins are part of the request itself, not a timing-dependent
    // Steam snapshot. This keeps every configured endpoint across refreshes.
    std::vector<uint64_t> pinned_endpoints(pinned_servers_.begin(), pinned_servers_.end());
    std::ranges::sort(pinned_endpoints);
    for (const auto endpoint : pinned_endpoints)
    {
        const auto ip = static_cast<uint32>(endpoint >> 16);
        const auto port = static_cast<uint16>(endpoint & 0xFFFFu);
        if (!endpoints.emplace(endpoint).second)
            continue;

        gameserveritem_t server{};
        InitEmptyGameServerItem(server, ip, port);
        servers.push_back(server);
    }

    return servers;
}

void MatchmakingSteamComp::RestartFavoriteRequests()
{
    for (auto& [request_id, request] : server_requests_)
    {
        if (!std::holds_alternative<ServerListRequestData>(request))
            continue;

        auto& request_data = std::get<ServerListRequestData>(request);
        if (!request_data.favorites_request)
            continue;

        request_data.cancellation_token->SetCanceled();
        request_data.cancellation_token = CancellationToken::Create();
        request_data.servers = BuildFavoriteServerList();
        request_data.in_progress = true;

        const auto response_callback = request_data.response_callback;
        const auto cancellation_token = request_data.cancellation_token;
        const auto servers = request_data.servers;
        TaskCoro::RunInMainThread([this, request_id, servers, response_callback, cancellation_token]() -> result<void>
        {
            cancellation_token->ThrowIfCancelled();
            co_await RefreshServerList(request_id, servers, response_callback, cancellation_token);
        });
    }
}

uint64_t MatchmakingSteamComp::MakePinnedServerKey(uint32 ip, uint16 port)
{
    return (static_cast<uint64_t>(ip) << 16) | port;
}

void MatchmakingSteamComp::CancelAllQueries()
{
    // Callbacks may mutate request state, so iterate over a stable snapshot.
    std::vector<HServerListRequest> request_ids;
    request_ids.reserve(server_requests_.size());
    for (const auto& [request_id, request] : server_requests_)
        request_ids.push_back(request_id);

    for (const auto request_id : request_ids)
        CancelQuery(request_id);
}

HServerListRequest MatchmakingSteamComp::RequestInternetServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto ct = CancellationToken::Create();
    auto servers_request_data = ServerListRequestData(request_id, response_callback, ct);

    server_requests_.emplace(request_id, std::move(servers_request_data));

    TaskCoro::RunInMainThread([this, request_id, response_callback, ct] () -> result<void>
    {
        ct->ThrowIfCancelled();
        co_await RequestServerList(request_id, MatchmakingService::ServerListSource::Internet, response_callback, ct);
    });

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse* response_callback)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestLANServerList(iApp, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestFriendsServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestFriendsServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestFavoritesServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto ct = CancellationToken::Create();
    auto request_data = ServerListRequestData(request_id, response_callback, ct);
    request_data.favorites_request = true;
    request_data.servers = BuildFavoriteServerList();
    auto servers = request_data.servers;
    server_requests_.emplace(request_id, std::move(request_data));

    TaskCoro::RunInMainThread([this, request_id, servers = std::move(servers), response_callback, ct]() -> result<void>
    {
        ct->ThrowIfCancelled();
        co_await RefreshServerList(request_id, servers, response_callback, ct);
    });

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestHistoryServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestHistoryServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestSpectatorServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestSpectatorServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

void MatchmakingSteamComp::ReleaseRequest(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);

        SteamMatchmakingServers()->CancelQuery(request_data.steam_request_id);
        SteamMatchmakingServers()->ReleaseRequest(request_data.steam_request_id);
        delete request_data.steam_response_callback;
    }
    else
    {
        auto& request_data = std::get<ServerListRequestData>(request);
        request_data.cancellation_token->SetCanceled();
    }

    server_requests_.erase(request_id);
}

gameserveritem_t* MatchmakingSteamComp::GetServerDetails(HServerListRequest request_id, int server_id)
{
    if (!server_requests_.contains(request_id))
    {
        return nullptr;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->GetServerDetails(request_data.steam_request_id, server_id);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    if (server_id < 0 || static_cast<size_t>(server_id) >= request_data.servers.size())
        return nullptr;

    return &request_data.servers[server_id];
}

void MatchmakingSteamComp::CancelQuery(HServerListRequest request_id)
{
    
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->CancelQuery(request_data.steam_request_id);
        return;
    }

    if (!IsRefreshing(request_id))
    {
        return;
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    request_data.in_progress = false;
    request_data.cancellation_token->SetCanceled();
    request_data.response_callback->RefreshComplete(request_id, eServerResponded);
}

void MatchmakingSteamComp::RefreshQuery(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->RefreshQuery(request_data.steam_request_id);
        return;
    }

    if (IsRefreshing(request_id))
    {
        return;
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    request_data.cancellation_token->SetCanceled();
    request_data.cancellation_token = CancellationToken::Create();
    request_data.in_progress = true;

    TaskCoro::RunInMainThread([this, request_id, ct = request_data.cancellation_token] () -> result<void>
    {
        ct->ThrowIfCancelled();

        const auto request_it = server_requests_.find(request_id);
        if (request_it == server_requests_.end() || !std::holds_alternative<ServerListRequestData>(request_it->second))
            co_return;

        const auto& request_data = std::get<ServerListRequestData>(request_it->second);
        co_await RefreshServerList(request_id, request_data.servers, request_data.response_callback, ct);
    });
}

bool MatchmakingSteamComp::IsRefreshing(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return false;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->IsRefreshing(request_data.steam_request_id);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    return request_data.in_progress;
}

int MatchmakingSteamComp::GetServerCount(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return 0;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->GetServerCount(request_data.steam_request_id);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    return request_data.servers.size();
}

void MatchmakingSteamComp::RefreshServer(HServerListRequest request_id, int server_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->RefreshServer(request_data.steam_request_id, server_id);
        return;
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    if (server_id < 0 || static_cast<size_t>(server_id) >= request_data.servers.size())
        return;

    TaskCoro::RunInMainThread([this](HServerListRequest request_id, int server_id, std::shared_ptr<CancellationToken> ct) -> result<void>
    {
        ct->ThrowIfCancelled();

        const auto request_it = server_requests_.find(request_id);
        if (request_it == server_requests_.end() || !std::holds_alternative<ServerListRequestData>(request_it->second))
            co_return;

        auto& initial_request = std::get<ServerListRequestData>(request_it->second);
        if (server_id < 0 || static_cast<size_t>(server_id) >= initial_request.servers.size())
            co_return;

        servernetadr_t net_addr = initial_request.servers[server_id].m_NetAdr;

        gameserveritem_t gameserver = co_await matchmaking_service_->RefreshServer(net_addr.GetIP(), net_addr.GetQueryPort());
        ct->ThrowIfCancelled();

        if (!server_requests_.contains(request_id))
        {
            co_return;
        }

        auto& request_data = std::get<ServerListRequestData>(server_requests_[request_id]);
        if (static_cast<size_t>(server_id) >= request_data.servers.size())
            co_return;

        if (gameserver.m_bHadSuccessfulResponse)
        {
            gameserver.m_ulTimeLastPlayed = request_data.servers[server_id].m_ulTimeLastPlayed;
            request_data.servers[server_id] = gameserver;

            request_data.response_callback->ServerResponded(request_id, server_id);
        }
        else
        {
            request_data.response_callback->ServerFailedToRespond(request_id, server_id);
        }
    }, request_id, server_id, request_data.cancellation_token);
}

HServerQuery MatchmakingSteamComp::PingServer(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response_callback)
{
    return SteamMatchmakingServers()->PingServer(ip, port, response_callback);
}

HServerQuery MatchmakingSteamComp::PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse* pRequestServersResponse)
{
    return SteamMatchmakingServers()->PlayerDetails(unIP, usPort, pRequestServersResponse);
}

HServerQuery MatchmakingSteamComp::ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse* pRequestServersResponse)
{
    return SteamMatchmakingServers()->ServerRules(unIP, usPort, pRequestServersResponse);
}

void MatchmakingSteamComp::CancelServerQuery(HServerQuery hServerQuery)
{
    SteamMatchmakingServers()->CancelServerQuery(hServerQuery);
}

result<void> MatchmakingSteamComp::RequestServerList(
    HServerListRequest request_id,
    MatchmakingService::ServerListSource server_list_source,
    ISteamMatchmakingServerListResponse* response_callback,
    std::shared_ptr<CancellationToken> ct
)
{
    co_await matchmaking_service_->RequestServerList(
        server_list_source,
        [this, request_id, response_callback, ct] (const MatchmakingService::ServerInfo& server_info)
        {
            if (ct->IsCanceled())
                return;
            ServerAnsweredHandler(request_id, response_callback, server_info);
        }, ct);

    ct->ThrowIfCancelled();
    const auto request_it = server_requests_.find(request_id);
    if (request_it == server_requests_.end() || !std::holds_alternative<ServerListRequestData>(request_it->second))
        co_return;

    std::get<ServerListRequestData>(request_it->second).in_progress = false;

    // server_requests_ and the response callback are main-thread confined
    assert(TaskCoro::IsMainThread());

    response_callback->RefreshComplete(request_id, eServerResponded);
}

result<void> MatchmakingSteamComp::RefreshServerList(
    HServerListRequest request_id,
    std::vector<gameserveritem_t> gameservers,
    ISteamMatchmakingServerListResponse* response_callback,
    std::shared_ptr<CancellationToken> ct
)
{
    co_await matchmaking_service_->RefreshServerList(
        gameservers,
        [this, request_id, response_callback, ct] (const MatchmakingService::ServerInfo& server_info)
        {
            if (ct->IsCanceled())
                return;
            ServerAnsweredHandler(request_id, response_callback, server_info);
        }, ct);

    ct->ThrowIfCancelled();
    const auto request_it = server_requests_.find(request_id);
    if (request_it == server_requests_.end() || !std::holds_alternative<ServerListRequestData>(request_it->second))
        co_return;

    std::get<ServerListRequestData>(request_it->second).in_progress = false;

    assert(TaskCoro::IsMainThread());

    response_callback->RefreshComplete(request_id, eServerResponded);
}

void MatchmakingSteamComp::ServerAnsweredHandler(
    HServerListRequest request_id,
    ISteamMatchmakingServerListResponse* response_callback,
    const MatchmakingService::ServerInfo& server_info)
{

    const auto request_it = server_requests_.find(request_id);
    if (request_it == server_requests_.end())
        return;

    auto& request = request_it->second;

    if (std::holds_alternative<ServerListRequestData>(request))
    {
        auto& request_data = std::get<ServerListRequestData>(request);

        size_t server_count = request_data.servers.size();

        if (server_info.server_index >= server_count)
        {
            request_data.servers.resize(server_info.server_index + 1);

            for (size_t i = server_count; i < request_data.servers.size(); ++i)
            {
                InitEmptyGameServerItem(request_data.servers[i], 0, 0);
            }
        }

        request_data.servers[server_info.server_index] = server_info.gameserver;
    }

    if (server_info.gameserver.m_bHadSuccessfulResponse)
    {
        response_callback->ServerResponded(request_id, server_info.server_index);
    }
    else
    {
        response_callback->ServerFailedToRespond(request_id, server_info.server_index);
    }
}

void MatchmakingSteamComp::InitEmptyGameServerItem(gameserveritem_t& gameserver, uint32_t ip, uint16_t port)
{

    if (app_id_ == 0)
    {
        app_id_ = SteamUtils()->GetAppID();
    }

    gameserver.m_NetAdr.Init(ip, port, port);
    gameserver.m_nAppID = app_id_;
    V_strcpy_safe(gameserver.m_szGameDir, "cstrike");
    V_strcpy_safe(gameserver.m_szMap, "-");
    V_strcpy_safe(gameserver.m_szGameDescription, "-");
}
