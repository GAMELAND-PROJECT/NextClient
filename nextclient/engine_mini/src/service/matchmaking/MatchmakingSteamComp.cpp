#include "MatchmakingSteamComp.h"
#include "engine.h"
#include "master/FileMasterClient.h"
#include "master/HttpMasterClient.h"
#include "sourcequery/SourceQueryInfo.h"
#include "sourcequery/source_query_constants.h"

#include <algorithm>
#include <cassert>
#include <optick.h>
#include <strtools.h>
#include <unordered_set>

#ifdef _WIN32
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#endif

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

namespace
{
constexpr char kPinnedServersUrl[] =
    "http://gameland.cam/pinned_servers.txt";
constexpr char kMixServersUrl[] =
    "http://gameland.cam/mix_servers.txt";
constexpr wchar_t kPinnedServersCacheFile[] = L"pinned_servers.dat";
constexpr wchar_t kMixServersCacheFile[] = L"mix_servers.dat";
constexpr size_t kMaxPinnedServers = 64;
constexpr size_t kMaxMixServers = 64;
constexpr uint32 kManagedOnlineMixMarker = 0x4D495858u; // "MIXX"
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

    {
        std::lock_guard<std::mutex> lock(active_queries_mutex_);
        for (auto& [id, ct] : active_queries_)
        {
            if (ct)
                ct->SetCanceled();
        }
        active_queries_.clear();
    }
}

void MatchmakingSteamComp::InitializePinnedServers()
{
    if (pinned_servers_initialized_)
        return;

    pinned_servers_initialized_ = true;
    pinned_cancellation_token_ = CancellationToken::Create();
    pinned_cache_client_ = std::make_shared<FileMasterClient>(kPinnedServersCacheFile);
    mix_cache_client_ = std::make_shared<FileMasterClient>(kMixServersCacheFile);
    pinned_http_client_ = std::make_shared<HttpMasterClient>(
        g_NextClientVersion,
        kPinnedServersUrl,
        true);
    mix_http_client_ = std::make_shared<HttpMasterClient>(
        g_NextClientVersion,
        kMixServersUrl,
        true);

    const auto cancellation_token = pinned_cancellation_token_;
    const auto cache_client = pinned_cache_client_;
    const auto http_client = pinned_http_client_;
    const auto mix_cache_client = mix_cache_client_;
    const auto mix_http_client = mix_http_client_;

    TaskCoro::RunInMainThread([this, cancellation_token, cache_client, http_client, mix_cache_client, mix_http_client]() -> result<void>
    {
        auto cached_addresses = co_await cache_client->GetServerAddressesAsync({}, cancellation_token);
        cancellation_token->ThrowIfCancelled();
        if (cached_addresses.size() > kMaxPinnedServers)
            cached_addresses.resize(kMaxPinnedServers);
        ApplyPinnedServers(cached_addresses);

        auto cached_mix_addresses = co_await mix_cache_client->GetServerAddressesAsync({}, cancellation_token);
        cancellation_token->ThrowIfCancelled();
        if (cached_mix_addresses.size() > kMaxMixServers)
            cached_mix_addresses.resize(kMaxMixServers);
        ApplyMixServers(cached_mix_addresses);

        RefreshManagedServerLists();
    });
}

bool MatchmakingSteamComp::IsPinnedServer(uint32 ip, uint16 port) const
{
    return online_endpoints_.Contains(ip, port);
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

    if (pinned_servers_ == updated_servers)
        return;

    pinned_servers_ = std::move(updated_servers);
    UpdateOnlineEndpointPins();
    RestartFavoriteRequests();
}

void MatchmakingSteamComp::ApplyMixServers(const std::vector<netadr_t>& addresses)
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

    if (mix_servers_ == updated_servers)
        return;

    mix_servers_ = std::move(updated_servers);
    UpdateOnlineEndpointPins();
    RestartFavoriteRequests();
}

void MatchmakingSteamComp::UpdateOnlineEndpointPins()
{
    std::unordered_set<uint64_t> online_servers;
    online_servers.reserve(pinned_servers_.size() + mix_servers_.size());
    online_servers.insert(pinned_servers_.begin(), pinned_servers_.end());
    online_servers.insert(mix_servers_.begin(), mix_servers_.end());
    online_endpoints_.UpdatePins(online_servers);
}

void MatchmakingSteamComp::RefreshManagedServerLists()
{
    if (!pinned_servers_initialized_ || !pinned_http_client_ || !pinned_cache_client_ ||
        !mix_http_client_ || !mix_cache_client_)
        return;

    if (managed_refresh_in_progress_)
        return;
    managed_refresh_in_progress_ = true;

    const auto cancellation_token = pinned_cancellation_token_;
    const auto cache_client = pinned_cache_client_;
    const auto http_client = pinned_http_client_;
    const auto mix_cache_client = mix_cache_client_;
    const auto mix_http_client = mix_http_client_;

    TaskCoro::RunInMainThread([this, cancellation_token, cache_client, http_client, mix_cache_client, mix_http_client]() -> result<void>
    {
        struct ManagedRefreshGuard
        {
            bool& value;
            ~ManagedRefreshGuard() { value = false; }
        } guard{managed_refresh_in_progress_};

        auto downloaded_addresses = co_await http_client->GetServerAddressesAsync({}, cancellation_token);
        cancellation_token->ThrowIfCancelled();

        // An empty response is treated as a failed/invalid update. It must not
        // erase a previously working cache or managed server list.
        if (!downloaded_addresses.empty())
        {
            if (downloaded_addresses.size() > kMaxPinnedServers)
                downloaded_addresses.resize(kMaxPinnedServers);

            ApplyPinnedServers(downloaded_addresses);
            co_await TaskCoro::RunIO([cache_client, downloaded_addresses]
            {
                cache_client->Save(downloaded_addresses);
            });
        }

        auto downloaded_mix_addresses = co_await mix_http_client->GetServerAddressesAsync({}, cancellation_token);
        cancellation_token->ThrowIfCancelled();
        if (!downloaded_mix_addresses.empty())
        {
            if (downloaded_mix_addresses.size() > kMaxMixServers)
                downloaded_mix_addresses.resize(kMaxMixServers);

            ApplyMixServers(downloaded_mix_addresses);
            co_await TaskCoro::RunIO([mix_cache_client, downloaded_mix_addresses]
            {
                mix_cache_client->Save(downloaded_mix_addresses);
            });
        }
    });
}

std::vector<gameserveritem_t> MatchmakingSteamComp::BuildFavoriteServerList()
{
    std::vector<gameserveritem_t> servers;
    servers.reserve(pinned_servers_.size() + mix_servers_.size());

    // Online/Favorites is an administratively managed list. Steam's local
    // user favorites are deliberately excluded so only remote pins appear.
    std::vector<uint64_t> pinned_endpoints(pinned_servers_.begin(), pinned_servers_.end());
    std::ranges::sort(pinned_endpoints);
    for (const auto endpoint : pinned_endpoints)
    {
        if (mix_servers_.contains(endpoint))
            continue;

        const auto ip = static_cast<uint32>(endpoint >> 16);
        const auto port = static_cast<uint16>(endpoint & 0xFFFFu);

        gameserveritem_t server{};
        InitEmptyGameServerItem(server, ip, port);
        servers.push_back(server);
    }

    std::vector<uint64_t> mix_endpoints(mix_servers_.begin(), mix_servers_.end());
    std::ranges::sort(mix_endpoints);
    for (const auto endpoint : mix_endpoints)
    {
        const auto ip = static_cast<uint32>(endpoint >> 16);
        const auto port = static_cast<uint16>(endpoint & 0xFFFFu);

        gameserveritem_t server{};
        InitEmptyGameServerItem(server, ip, port);
        server.m_ulTimeLastPlayed = kManagedOnlineMixMarker;
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

    {
        std::lock_guard<std::mutex> lock(active_queries_mutex_);
        for (auto& [id, ct] : active_queries_)
        {
            if (ct)
                ct->SetCanceled();
        }
        active_queries_.clear();
    }
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

    auto ct = CancellationToken::Create();
    auto request_data = ServerListRequestData(request_id, response_callback, ct);
    request_data.in_progress = true;
    request_data.lan_request = true;
    server_requests_.emplace(request_id, std::move(request_data));

    TaskCoro::RunInMainThread([this, request_id, response_callback, ct]() -> result<void>
    {
        ct->ThrowIfCancelled();
        co_await QueryLanServers(request_id, response_callback, ct);
    });

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

    RefreshManagedServerLists();

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
    if (request_data.favorites_request)
        RefreshManagedServerLists();

    request_data.cancellation_token->SetCanceled();
    request_data.cancellation_token = CancellationToken::Create();
    request_data.in_progress = true;

    if (request_data.lan_request)
    {
        TaskCoro::RunInMainThread([this, request_id, ct = request_data.cancellation_token]() -> result<void>
        {
            ct->ThrowIfCancelled();

            const auto request_it = server_requests_.find(request_id);
            if (request_it == server_requests_.end() || !std::holds_alternative<ServerListRequestData>(request_it->second))
                co_return;

            const auto& request_data = std::get<ServerListRequestData>(request_it->second);
            co_await QueryLanServers(request_id, request_data.response_callback, ct);
        });
        return;
    }

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
            online_endpoints_.Observe(gameserver.m_NetAdr.GetIP(),
                gameserver.m_NetAdr.GetQueryPort(), gameserver.m_NetAdr.GetConnectionPort());
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
    if (!response_callback || !source_query_)
        return 0;

    HServerQuery query_id = (HServerQuery)++server_query_counter_;
    auto ct = CancellationToken::Create();
    {
        std::lock_guard<std::mutex> lock(active_queries_mutex_);
        active_queries_[query_id] = ct;
    }

    netadr_t addr(ip, port);

    TaskCoro::RunInMainThread([this, query_id, addr, response_callback, ct]() -> result<void>
    {
        try
        {
            auto resp = co_await source_query_->GetInfoAsync(addr);
            if (ct->IsCanceled())
                co_return;

            {
                std::lock_guard<std::mutex> lock(active_queries_mutex_);
                active_queries_.erase(query_id);
            }

            if (resp.error_code == SQErrorCode::Ok)
            {
                gameserveritem_t server = MatchmakingService::ConvertToGameServerItem(resp);
                response_callback->ServerResponded(server);
            }
            else
            {
                response_callback->ServerFailedToRespond();
            }
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(active_queries_mutex_);
                active_queries_.erase(query_id);
            }
            if (!ct->IsCanceled())
                response_callback->ServerFailedToRespond();
        }
        co_return;
    });

    return query_id;
}

HServerQuery MatchmakingSteamComp::PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse* pRequestServersResponse)
{
    if (!pRequestServersResponse || !source_query_)
        return 0;

    HServerQuery query_id = (HServerQuery)++server_query_counter_;
    auto ct = CancellationToken::Create();
    {
        std::lock_guard<std::mutex> lock(active_queries_mutex_);
        active_queries_[query_id] = ct;
    }

    netadr_t addr(unIP, usPort);

    TaskCoro::RunInMainThread([this, query_id, addr, pRequestServersResponse, ct]() -> result<void>
    {
        try
        {
            auto resp = co_await source_query_->GetPlayersAsync(addr);
            if (ct->IsCanceled())
                co_return;

            {
                std::lock_guard<std::mutex> lock(active_queries_mutex_);
                active_queries_.erase(query_id);
            }

            if (resp.error_code == SQErrorCode::Ok)
            {
                for (const auto& player : resp.value)
                {
                    if (ct->IsCanceled())
                        co_return;
                    pRequestServersResponse->AddPlayerToList(player.player_name.c_str(), player.kills, player.time_connected);
                }
                pRequestServersResponse->PlayersRefreshComplete();
            }
            else
            {
                pRequestServersResponse->PlayersFailedToRespond();
            }
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(active_queries_mutex_);
                active_queries_.erase(query_id);
            }
            if (!ct->IsCanceled())
                pRequestServersResponse->PlayersFailedToRespond();
        }
        co_return;
    });

    return query_id;
}

HServerQuery MatchmakingSteamComp::ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse* pRequestServersResponse)
{
    if (!pRequestServersResponse || !source_query_)
        return 0;

    HServerQuery query_id = (HServerQuery)++server_query_counter_;
    auto ct = CancellationToken::Create();
    {
        std::lock_guard<std::mutex> lock(active_queries_mutex_);
        active_queries_[query_id] = ct;
    }

    netadr_t addr(unIP, usPort);

    TaskCoro::RunInMainThread([this, query_id, addr, pRequestServersResponse, ct]() -> result<void>
    {
        try
        {
            auto resp = co_await source_query_->GetRulesAsync(addr);
            if (ct->IsCanceled())
                co_return;

            {
                std::lock_guard<std::mutex> lock(active_queries_mutex_);
                active_queries_.erase(query_id);
            }

            if (resp.error_code == SQErrorCode::Ok)
            {
                for (const auto& rule : resp.value)
                {
                    if (ct->IsCanceled())
                        co_return;
                    pRequestServersResponse->RulesResponded(rule.name.c_str(), rule.value.c_str());
                }
                pRequestServersResponse->RulesRefreshComplete();
            }
            else
            {
                pRequestServersResponse->RulesFailedToRespond();
            }
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(active_queries_mutex_);
                active_queries_.erase(query_id);
            }
            if (!ct->IsCanceled())
                pRequestServersResponse->RulesFailedToRespond();
        }
        co_return;
    });

    return query_id;
}

void MatchmakingSteamComp::CancelServerQuery(HServerQuery hServerQuery)
{
    std::lock_guard<std::mutex> lock(active_queries_mutex_);
    auto it = active_queries_.find(hServerQuery);
    if (it != active_queries_.end())
    {
        if (it->second)
            it->second->SetCanceled();
        active_queries_.erase(it);
    }
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

result<void> MatchmakingSteamComp::QueryLanServers(
    HServerListRequest request_id,
    ISteamMatchmakingServerListResponse* response_callback,
    std::shared_ptr<CancellationToken> ct
)
{
    std::vector<gameserveritem_t> discovered = co_await TaskCoro::RunIO([ct]() -> std::vector<gameserveritem_t>
    {
        std::vector<gameserveritem_t> servers;
        std::vector<SOCKET> sockets;

        auto createSocket = [](uint32_t bindIp) -> SOCKET
        {
            SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET)
                return INVALID_SOCKET;

            int broadcastOpt = 1;
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcastOpt), sizeof(broadcastOpt));

            int bufSize = 128 * 1024;
            setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
            setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));

#ifdef _WIN32
            BOOL bNewBehavior = FALSE;
            DWORD dwBytesReturned = 0;
            WSAIoctl(s, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

            sockaddr_in bindAddr{};
            bindAddr.sin_family = AF_INET;
            bindAddr.sin_addr.s_addr = bindIp;
            bindAddr.sin_port = 0;
            if (bind(s, reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR)
            {
                closesocket(s);
                return INVALID_SOCKET;
            }

            return s;
        };

        // 1. Primary socket bound to INADDR_ANY (0.0.0.0)
        SOCKET mainSock = createSocket(INADDR_ANY);
        if (mainSock != INVALID_SOCKET)
            sockets.push_back(mainSock);

        // 2. Discover local broadcast addresses, adapter sockets, and subnet IP ranges
        std::unordered_set<uint32_t> broadcastIps;
        broadcastIps.insert(INADDR_BROADCAST); // 255.255.255.255
        broadcastIps.insert(htonl(INADDR_LOOPBACK)); // 127.0.0.1

        struct AdapterSubnet
        {
            uint32_t baseNet{}; // host byte order
            uint32_t hostCount{};
            SOCKET sock{ INVALID_SOCKET };
        };
        std::vector<AdapterSubnet> subnets;

#ifdef _WIN32
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD bufLen = sizeof(adapterInfo);
        if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_SUCCESS)
        {
            PIP_ADAPTER_INFO pAdapter = adapterInfo;
            while (pAdapter)
            {
                IP_ADDR_STRING* pIp = &pAdapter->IpAddressList;
                while (pIp)
                {
                    if (pIp->IpAddress.String[0] &&
                        strcmp(pIp->IpAddress.String, "0.0.0.0") != 0 &&
                        strcmp(pIp->IpAddress.String, "127.0.0.1") != 0)
                    {
                        unsigned long ipNet = inet_addr(pIp->IpAddress.String);
                        unsigned long maskNet = inet_addr(pIp->IpMask.String);
                        if (ipNet != INADDR_NONE && maskNet != INADDR_NONE)
                        {
                            unsigned long bcastNet = (ipNet & maskNet) | (~maskNet);
                            broadcastIps.insert(bcastNet);

                            SOCKET s = createSocket(ipNet);
                            if (s != INVALID_SOCKET)
                                sockets.push_back(s);

                            uint32_t ipHost = ntohl(ipNet);
                            uint32_t maskHost = ntohl(maskNet);
                            uint32_t netHost = ipHost & maskHost;
                            uint32_t invMask = ~maskHost;

                            // If subnet is /24 or smaller (up to 512 hosts), prepare unicast sweep
                            if (invMask > 0 && invMask <= 512)
                            {
                                AdapterSubnet sub{};
                                sub.baseNet = netHost;
                                sub.hostCount = invMask;
                                sub.sock = (s != INVALID_SOCKET) ? s : mainSock;
                                subnets.push_back(sub);
                            }
                        }
                    }
                    pIp = pIp->Next;
                }
                pAdapter = pAdapter->Next;
            }
        }
#endif

        if (sockets.empty())
            return servers;

        // Ports typically used by listen servers
        const uint16_t targetPorts[] = { 27015, 27016, 27017, 27018, 27019, 27020, 27005 };

        // Query payloads: A2S_INFO, infostring, details, ping
        const char a2sPayload[25] = { '\xFF', '\xFF', '\xFF', '\xFF', 'T', 'S', 'o', 'u', 'r', 'c', 'e', ' ', 'E', 'n', 'g', 'i', 'n', 'e', ' ', 'Q', 'u', 'e', 'r', 'y', '\0' };
        const char infoStringPayload[15] = { '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', 'o', 's', 't', 'r', 'i', 'n', 'g', '\0' };
        const char detailsPayload[12] = { '\xFF', '\xFF', '\xFF', '\xFF', 'd', 'e', 't', 'a', 'i', 'l', 's', '\0' };
        const char pingPayload[9] = { '\xFF', '\xFF', '\xFF', '\xFF', 'p', 'i', 'n', 'g', '\0' };

        auto sendQueryBurst = [&]()
        {
            // 1. Send broadcast queries to all discovered broadcast endpoints
            for (uint32_t bcastIp : broadcastIps)
            {
                for (uint16_t port : targetPorts)
                {
                    sockaddr_in target{};
                    target.sin_family = AF_INET;
                    target.sin_addr.s_addr = bcastIp;
                    target.sin_port = htons(port);

                    for (SOCKET s : sockets)
                    {
                        sendto(s, a2sPayload, sizeof(a2sPayload), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
                        sendto(s, infoStringPayload, sizeof(infoStringPayload), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
                        sendto(s, pingPayload, sizeof(pingPayload), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
                    }
                }
            }

            // 2. Unicast sweep across local subnets (bypasses router broadcast drops and Wi-Fi AP isolation)
            for (const auto& sub : subnets)
            {
                SOCKET s = (sub.sock != INVALID_SOCKET) ? sub.sock : sockets[0];
                for (uint32_t h = 1; h < sub.hostCount; ++h)
                {
                    uint32_t targetIpHost = sub.baseNet | h;
                    uint32_t targetIpNet = htonl(targetIpHost);

                    for (uint16_t port : { static_cast<uint16_t>(27015), static_cast<uint16_t>(27016), static_cast<uint16_t>(27017) })
                    {
                        sockaddr_in target{};
                        target.sin_family = AF_INET;
                        target.sin_addr.s_addr = targetIpNet;
                        target.sin_port = htons(port);

                        sendto(s, a2sPayload, sizeof(a2sPayload), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
                        sendto(s, infoStringPayload, sizeof(infoStringPayload), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
                    }
                }
            }
        };

        // First burst
        sendQueryBurst();

        auto startTime = std::chrono::steady_clock::now();
        std::unordered_set<uint64_t> seenEndpoints;
        bool retrySent = false;

        while (!ct->IsCanceled())
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
            if (elapsed > 1800)
                break;

            if (!retrySent && elapsed >= 300)
            {
                retrySent = true;
                sendQueryBurst();
            }

            fd_set readFds;
            FD_ZERO(&readFds);
            for (SOCKET s : sockets)
                FD_SET(s, &readFds);

            timeval tv{ 0, 20000 }; // 20ms
            int sel = select(0, &readFds, nullptr, nullptr, &tv);
            if (sel <= 0)
                continue;

            for (SOCKET s : sockets)
            {
                if (!FD_ISSET(s, &readFds))
                    continue;

                sockaddr_in fromAddr{};
                int fromLen = sizeof(fromAddr);
                char buf[2048];
                int bytesRecv = recvfrom(s, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
                if (bytesRecv <= 4)
                    continue;

                if (static_cast<unsigned char>(buf[0]) != 0xFF || static_cast<unsigned char>(buf[1]) != 0xFF ||
                    static_cast<unsigned char>(buf[2]) != 0xFF || static_cast<unsigned char>(buf[3]) != 0xFF)
                    continue;

                uint32_t fromIp = ntohl(fromAddr.sin_addr.s_addr);
                uint16_t fromPort = ntohs(fromAddr.sin_port);
                if (fromPort == 0)
                    continue;

                uint64_t epKey = (static_cast<uint64_t>(fromIp) << 16) | fromPort;
                if (seenEndpoints.contains(epKey))
                    continue;

                unsigned char headerType = static_cast<unsigned char>(buf[4]);

                // Challenge handshake
                if (headerType == 'A') // S2C_CHALLENGE
                {
                    if (bytesRecv >= 9)
                    {
                        char challengeReply[29];
                        memcpy(challengeReply, a2sPayload, 25);
                        memcpy(challengeReply + 25, buf + 5, 4);
                        sendto(s, challengeReply, sizeof(challengeReply), 0, reinterpret_cast<const sockaddr*>(&fromAddr), fromLen);
                    }
                    continue;
                }

                SQ_INFO sqInfo{};
                bool parsed = false;

                if (headerType == 'I') // S2A_INFO (Source)
                {
                    try
                    {
                        ByteBuffer byteBuf(reinterpret_cast<const uint8_t*>(buf + 5), static_cast<size_t>(bytesRecv - 5));
                        sqInfo = SourceQueryInfo::ParseInfo(byteBuf);
                        parsed = true;
                    }
                    catch (...)
                    {
                    }
                }
                else if (headerType == 'm') // S2A_RULES_GS (GoldSrc Detailed)
                {
                    try
                    {
                        ByteBuffer byteBuf(reinterpret_cast<const uint8_t*>(buf + 5), static_cast<size_t>(bytesRecv - 5));
                        sqInfo = SourceQueryInfo::ParseRulesGs(byteBuf);
                        parsed = true;
                    }
                    catch (...)
                    {
                    }
                }

                // If not parsed yet, check for text infostring response
                if (!parsed && bytesRecv > 5)
                {
                    std::string text(buf + 4, bytesRecv - 4);
                    if (text.find('\\') != std::string::npos)
                    {
                        size_t pos = text.find('\\');
                        std::unordered_map<std::string, std::string> kv;
                        while (pos < text.size())
                        {
                            if (text[pos] != '\\') { pos++; continue; }
                            pos++;
                            size_t nextSlash = text.find('\\', pos);
                            if (nextSlash == std::string::npos) break;
                            std::string key = text.substr(pos, nextSlash - pos);
                            pos = nextSlash + 1;
                            size_t valEnd = text.find('\\', pos);
                            std::string val = (valEnd == std::string::npos) ? text.substr(pos) : text.substr(pos, valEnd - pos);
                            pos = (valEnd == std::string::npos) ? text.size() : valEnd;
                            std::ranges::transform(key, key.begin(), ::tolower);
                            kv[key] = val;
                        }

                        if (!kv.empty())
                        {
                            if (kv.contains("hostname")) sqInfo.hostname = kv["hostname"];
                            if (kv.contains("map")) sqInfo.map = kv["map"];
                            if (kv.contains("gamedir")) sqInfo.game_directory = kv["gamedir"];
                            if (kv.contains("description")) sqInfo.game_description = kv["description"];
                            if (kv.contains("players")) sqInfo.num_players = std::atoi(kv["players"].c_str());
                            if (kv.contains("max")) sqInfo.max_players = std::atoi(kv["max"].c_str());
                            if (kv.contains("password")) sqInfo.password = (std::atoi(kv["password"].c_str()) != 0);
                            parsed = true;
                        }
                    }
                }

                // Fallback for any GoldSrc response (e.g. 'C', 'j' ping, or raw reply)
                if (!parsed && bytesRecv >= 5)
                {
                    sqInfo.hostname = "Counter-Strike LAN Server";
                    sqInfo.game_directory = "cstrike";
                    sqInfo.map = "-";
                    sqInfo.max_players = 32;
                    sqInfo.num_players = 1;
                    parsed = true;
                }

                if (parsed)
                {
                    gameserveritem_t item{};
                    item.m_NetAdr.Init(fromIp, fromPort, fromPort);
                    item.m_nAppID = 10;
                    if (sqInfo.port != 0)
                        item.m_NetAdr.SetConnectionPort(sqInfo.port);

                    if (!sqInfo.hostname.empty())
                        item.SetName(sqInfo.hostname.c_str());
                    else
                        item.SetName("Counter-Strike LAN Server");

                    item.m_bPassword = sqInfo.password;
                    item.m_bSecure = sqInfo.secure;
                    item.m_nBotPlayers = sqInfo.num_of_bots;
                    item.m_nMaxPlayers = (sqInfo.max_players > 0) ? sqInfo.max_players : 32;
                    item.m_nPlayers = sqInfo.num_players;
                    item.m_nPing = static_cast<int>(std::clamp(static_cast<long long>(elapsed), 1LL, 20LL));
                    item.m_bHadSuccessfulResponse = true;
                    item.m_bDoNotRefresh = false;

                    if (!sqInfo.game_directory.empty())
                        V_strcpy_safe(item.m_szGameDir, sqInfo.game_directory.c_str());
                    else
                        V_strcpy_safe(item.m_szGameDir, "cstrike");

                    if (!sqInfo.map.empty())
                        V_strcpy_safe(item.m_szMap, sqInfo.map.c_str());
                    else
                        V_strcpy_safe(item.m_szMap, "-");

                    if (!sqInfo.game_description.empty())
                        V_strcpy_safe(item.m_szGameDescription, sqInfo.game_description.c_str());
                    else
                        V_strcpy_safe(item.m_szGameDescription, "Counter-Strike");

                    seenEndpoints.insert(epKey);
                    servers.push_back(item);
                }
            }
        }

        for (SOCKET s : sockets)
            closesocket(s);

        return servers;
    });

    ct->ThrowIfCancelled();

    const auto request_it = server_requests_.find(request_id);
    if (request_it == server_requests_.end() || !std::holds_alternative<ServerListRequestData>(request_it->second))
        co_return;

    auto& req_data = std::get<ServerListRequestData>(request_it->second);
    req_data.servers = std::move(discovered);
    req_data.in_progress = false;

    assert(TaskCoro::IsMainThread());

    for (size_t i = 0; i < req_data.servers.size(); ++i)
    {
        response_callback->ServerResponded(request_id, static_cast<int>(i));
    }

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

        const auto online_category_marker = request_data.servers[server_info.server_index].m_ulTimeLastPlayed;
        request_data.servers[server_info.server_index] = server_info.gameserver;
        request_data.servers[server_info.server_index].m_ulTimeLastPlayed = online_category_marker;
    }

    if (server_info.gameserver.m_bHadSuccessfulResponse)
    {
        const auto& address = server_info.gameserver.m_NetAdr;
        online_endpoints_.Observe(address.GetIP(), address.GetQueryPort(), address.GetConnectionPort());
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
