#pragma once

#include <steam/isteamfriends.h>
#include <steam/isteamapps.h>
#include <steam/isteamgameserver.h>
#include <steam/isteamutils.h>
#include <steam/isteammatchmaking.h>
#include <steam/isteamhttp.h>
#include "SteamEmu.h"
#include <ctime>
#include <string>
#include <vector>
#include <queue>
#include <mutex>

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamFriends
//-----------------------------------------------------------------------------
class SteamFriendsStub : public ISteamFriends
{
public:
    const char *GetPersonaName() override { return "Player"; }
    SteamAPICall_t SetPersonaName(const char *) override { return k_uAPICallInvalid; }
    EPersonaState GetPersonaState() override { return k_EPersonaStateOnline; }
    int GetFriendCount(int) override { return 0; }
    CSteamID GetFriendByIndex(int, int) override { return CSteamID(); }
    EFriendRelationship GetFriendRelationship(CSteamID) override { return k_EFriendRelationshipNone; }
    EPersonaState GetFriendPersonaState(CSteamID) override { return k_EPersonaStateOffline; }
    const char *GetFriendPersonaName(CSteamID) override { return ""; }
    bool GetFriendGamePlayed(CSteamID, FriendGameInfo_t *) override { return false; }
    const char *GetFriendPersonaNameHistory(CSteamID, int) override { return ""; }
    bool HasFriend(CSteamID, int) override { return false; }
    int GetClanCount() override { return 0; }
    CSteamID GetClanByIndex(int) override { return CSteamID(); }
    const char *GetClanName(CSteamID) override { return ""; }
    const char *GetClanTag(CSteamID) override { return ""; }
    bool GetClanActivityCounts(CSteamID, int *, int *, int *) override { return false; }
    SteamAPICall_t DownloadClanActivityCounts(CSteamID *, int) override { return k_uAPICallInvalid; }
    int GetFriendCountFromSource(CSteamID) override { return 0; }
    CSteamID GetFriendFromSourceByIndex(CSteamID, int) override { return CSteamID(); }
    bool IsUserInSource(CSteamID, CSteamID) override { return false; }
    void SetInGameVoiceSpeaking(CSteamID, bool) override {}
    void ActivateGameOverlay(const char *) override {}
    void ActivateGameOverlayToUser(const char *, CSteamID) override {}
    void ActivateGameOverlayToWebPage(const char *) override {}
    void ActivateGameOverlayToStore(AppId_t, EOverlayToStoreFlag) override {}
    void SetPlayedWith(CSteamID) override {}
    void ActivateGameOverlayInviteDialog(CSteamID) override {}
    int GetSmallFriendAvatar(CSteamID) override { return 0; }
    int GetMediumFriendAvatar(CSteamID) override { return 0; }
    int GetLargeFriendAvatar(CSteamID) override { return 0; }
    bool RequestUserInformation(CSteamID, bool) override { return false; }
    SteamAPICall_t RequestClanOfficerList(CSteamID) override { return k_uAPICallInvalid; }
    CSteamID GetClanOwner(CSteamID) override { return CSteamID(); }
    int GetClanOfficerCount(CSteamID) override { return 0; }
    CSteamID GetClanOfficerByIndex(CSteamID, int) override { return CSteamID(); }
    uint32 GetUserRestrictions() override { return 0; }
    bool SetRichPresence(const char *, const char *) override { return false; }
    void ClearRichPresence() override {}
    const char *GetFriendRichPresence(CSteamID, const char *) override { return ""; }
    int GetFriendRichPresenceKeyCount(CSteamID) override { return 0; }
    const char *GetFriendRichPresenceKeyByIndex(CSteamID, int) override { return ""; }
    void RequestFriendRichPresence(CSteamID) override {}
    bool InviteUserToGame(CSteamID, const char *) override { return false; }
    int GetCoplayFriendCount() override { return 0; }
    CSteamID GetCoplayFriend(int) override { return CSteamID(); }
    int GetFriendCoplayTime(CSteamID) override { return 0; }
    AppId_t GetFriendCoplayGame(CSteamID) override { return 0; }
    SteamAPICall_t JoinClanChatRoom(CSteamID) override { return k_uAPICallInvalid; }
    bool LeaveClanChatRoom(CSteamID) override { return false; }
    int GetClanChatMemberCount(CSteamID) override { return 0; }
    CSteamID GetChatMemberByIndex(CSteamID, int) override { return CSteamID(); }
    bool SendClanChatMessage(CSteamID, const char *) override { return false; }
    int GetClanChatMessage(CSteamID, int, void *, int, EChatEntryType *, CSteamID *) override { return 0; }
    bool IsClanChatAdmin(CSteamID, CSteamID) override { return false; }
    bool IsClanChatWindowOpenInSteam(CSteamID) override { return false; }
    bool OpenClanChatWindowInSteam(CSteamID) override { return false; }
    bool CloseClanChatWindowInSteam(CSteamID) override { return false; }
    bool SetListenForFriendsMessages(bool) override { return false; }
    bool ReplyToFriendMessage(CSteamID, const char *) override { return false; }
    int GetFriendMessage(CSteamID, int, void *, int, EChatEntryType *) override { return 0; }
    SteamAPICall_t GetFollowerCount(CSteamID) override { return k_uAPICallInvalid; }
    SteamAPICall_t IsFollowing(CSteamID) override { return k_uAPICallInvalid; }
    SteamAPICall_t EnumerateFollowingList(uint32) override { return k_uAPICallInvalid; }
};

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamApps
//-----------------------------------------------------------------------------
class SteamAppsStub : public ISteamApps
{
public:
    bool BIsSubscribed() override { return true; }
    bool BIsLowViolence() override { return false; }
    bool BIsCybercafe() override { return false; }
    bool BIsVACBanned() override { return false; }
    const char *GetCurrentGameLanguage() override { return "english"; }
    const char *GetAvailableGameLanguages() override { return "english"; }
    bool BIsSubscribedApp(AppId_t) override { return true; }
    bool BIsDlcInstalled(AppId_t) override { return false; }
    uint32 GetEarliestPurchaseUnixTime(AppId_t) override { return 0; }
    bool BIsSubscribedFromFreeWeekend() override { return false; }
    int GetDLCCount() override { return 0; }
    bool BGetDLCDataByIndex(int, AppId_t *, bool *, char *, int) override { return false; }
    void InstallDLC(AppId_t) override {}
    void UninstallDLC(AppId_t) override {}
    void RequestAppProofOfPurchaseKey(AppId_t) override {}
    bool GetCurrentBetaName(char *, int) override { return false; }
    bool MarkContentCorrupt(bool) override { return false; }
    uint32 GetInstalledDepots(DepotId_t *, uint32) override { return 0; }
    uint32 GetAppInstallDir(AppId_t, char *, uint32) override { return 0; }
#ifdef _PS3
    SteamAPICall_t RegisterActivationCode(const char *) override { return k_uAPICallInvalid; }
#endif
};

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamGameServer
//-----------------------------------------------------------------------------
class SteamGameServerStub : public ISteamGameServer
{
private:
    struct OutgoingPacket {
        std::vector<uint8_t> payload;
        uint32_t ip{};
        uint16_t port{};
    };

    std::mutex mutex_;
    std::queue<OutgoingPacket> outgoing_packets_;

    bool is_active_{ false };
    std::string server_name_{ "Counter-Strike 1.6 Server" };
    std::string map_name_{ "" };
    std::string mod_dir_{ "cstrike" };
    std::string game_desc_{ "Counter-Strike" };
    int max_players_{ 32 };
    int bot_players_{ 0 };
    int players_{ 0 };
    bool password_protected_{ false };
    bool dedicated_{ false };
    uint16_t game_port_{ 27015 };
    uint16_t query_port_{ 27015 };

public:
    void SetGamePort(uint16_t port) { game_port_ = port; }
    void SetQueryPort(uint16_t port) { query_port_ = port; }

    void Shutdown()
    {
        is_active_ = false;
        map_name_.clear();
        players_ = 0;
        bot_players_ = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        while (!outgoing_packets_.empty())
            outgoing_packets_.pop();
    }

    bool InitGameServer(uint32, uint16 usGamePort, uint16 usQueryPort, uint32, AppId_t, const char *) override
    {
        if (usGamePort != 0) game_port_ = usGamePort;
        if (usQueryPort != 0) query_port_ = usQueryPort;
        return true;
    }

    void SetProduct(const char *) override {}
    void SetGameDescription(const char *desc) override
    {
        if (desc && desc[0]) game_desc_ = desc;
    }
    void SetModDir(const char *mod) override
    {
        if (mod && mod[0]) mod_dir_ = mod;
    }
    void SetDedicatedServer(bool dedicated) override
    {
        dedicated_ = dedicated;
    }
    void LogOn(const char *, const char *) override { is_active_ = true; }
    void LogOnAnonymous() override { is_active_ = true; }
    void LogOff() override
    {
        is_active_ = false;
        map_name_.clear();
        players_ = 0;
        bot_players_ = 0;
    }
    bool BLoggedOn() override { return is_active_; }
    bool BSecure() override { return false; }
    CSteamID GetSteamID() override { return SteamEmu::GetSteamID(); }
    bool WasRestartRequested() override { return false; }
    void SetMaxPlayerCount(int count) override
    {
        if (count > 0) max_players_ = count;
    }
    void SetBotPlayerCount(int count) override
    {
        bot_players_ = count;
    }
    void SetServerName(const char *name) override
    {
        if (name && name[0]) server_name_ = name;
    }
    void SetMapName(const char *map) override
    {
        if (map && map[0] && strcmp(map, "-") != 0)
        {
            map_name_ = map;
            is_active_ = true;
            players_ = 0;
            bot_players_ = 0;
        }
        else
        {
            map_name_.clear();
            is_active_ = false;
            players_ = 0;
            bot_players_ = 0;
        }
    }
    void SetPasswordProtected(bool pwd) override
    {
        password_protected_ = pwd;
    }
    void SetSpectatorPort(uint16) override {}
    void SetSpectatorServerName(const char *) override {}
    void ClearAllKeyValues() override {}
    void SetKeyValue(const char *, const char *) override {}
    void SetGameTags(const char *) override {}
    void SetGameData(const char *) override {}
    void SetRegion(const char *) override {}

    bool SendUserConnectAndAuthenticate(uint32, const void *, uint32, CSteamID *pSteamIDUser) override
    {
        players_++;
        if (players_ > max_players_) players_ = max_players_;
        if (pSteamIDUser) *pSteamIDUser = SteamEmu::GetSteamID();
        return true;
    }

    CSteamID CreateUnauthenticatedUserConnection() override { return SteamEmu::GetSteamID(); }

    void SendUserDisconnect(CSteamID) override
    {
        if (players_ > 0) players_--;
    }

    bool BUpdateUserData(CSteamID, const char *, uint32) override { return true; }
    HAuthTicket GetAuthSessionTicket(void *, int, uint32 *pcbTicket) override
    {
        if (pcbTicket) *pcbTicket = 0;
        return k_HAuthTicketInvalid;
    }
    EBeginAuthSessionResult BeginAuthSession(const void *, int, CSteamID) override { return k_EBeginAuthSessionResultOK; }
    void EndAuthSession(CSteamID) override {}
    void CancelAuthTicket(HAuthTicket) override {}
    EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID, AppId_t) override { return k_EUserHasLicenseResultHasLicense; }
    bool RequestUserGroupStatus(CSteamID, CSteamID) override { return false; }
    void GetGameplayStats() override {}
    SteamAPICall_t GetServerReputation() override { return k_uAPICallInvalid; }
    uint32 GetPublicIP() override { return 0; }

    bool HandleIncomingPacket(const void *pData, int cbData, uint32 srcIP, uint16 srcPort) override
    {
        if (!is_active_ || map_name_.empty() || map_name_ == "-")
            return false;

        if (!pData || cbData < 5)
            return false;

        const uint8_t *bytes = reinterpret_cast<const uint8_t*>(pData);
        if (bytes[0] != 0xFF || bytes[1] != 0xFF || bytes[2] != 0xFF || bytes[3] != 0xFF)
            return false;

        uint8_t type = bytes[4];

        // 1. A2S_INFO query: '\xFF\xFF\xFF\xFFTSource Engine Query\0'
        if (type == 'T')
        {
            std::vector<uint8_t> out;
            out.reserve(256);
            out.push_back(0xFF); out.push_back(0xFF); out.push_back(0xFF); out.push_back(0xFF);
            out.push_back('I');
            out.push_back(17); // protocol version

            for (char c : server_name_) out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);

            for (char c : map_name_) out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);

            for (char c : mod_dir_) out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);

            for (char c : game_desc_) out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);

            int16_t appId = 10;
            out.push_back(static_cast<uint8_t>(appId & 0xFF));
            out.push_back(static_cast<uint8_t>((appId >> 8) & 0xFF));

            out.push_back(static_cast<uint8_t>(players_));
            out.push_back(static_cast<uint8_t>(max_players_));
            out.push_back(static_cast<uint8_t>(bot_players_));
            out.push_back(static_cast<uint8_t>(dedicated_ ? 'd' : 'l'));
            out.push_back(static_cast<uint8_t>('w'));
            out.push_back(static_cast<uint8_t>(password_protected_ ? 1 : 0));
            out.push_back(0); // secure

            std::string ver = "1.1.2.6";
            for (char c : ver) out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);

            // EDF: 0x80 (includes game port)
            out.push_back(0x80);
            out.push_back(static_cast<uint8_t>(game_port_ & 0xFF));
            out.push_back(static_cast<uint8_t>((game_port_ >> 8) & 0xFF));

            std::lock_guard<std::mutex> lock(mutex_);
            outgoing_packets_.push({ std::move(out), srcIP, srcPort });
            return true;
        }

        // 2. Infostring / details query: '\xFF\xFF\xFF\xFFinfostring\0'
        if (type == 'i' || type == 'd')
        {
            std::string kv = "\\infostring\\";
            kv += "\\hostname\\" + server_name_;
            kv += "\\map\\" + map_name_;
            kv += "\\gamedir\\" + mod_dir_;
            kv += "\\description\\" + game_desc_;
            kv += "\\players\\" + std::to_string(players_);
            kv += "\\max\\" + std::to_string(max_players_);
            kv += "\\password\\" + std::string(password_protected_ ? "1" : "0");

            std::vector<uint8_t> out;
            out.push_back(0xFF); out.push_back(0xFF); out.push_back(0xFF); out.push_back(0xFF);
            for (char c : kv) out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);

            std::lock_guard<std::mutex> lock(mutex_);
            outgoing_packets_.push({ std::move(out), srcIP, srcPort });
            return true;
        }

        // 3. Ping query: '\xFF\xFF\xFF\xFFping\0'
        if (type == 'p')
        {
            std::vector<uint8_t> out = { 0xFF, 0xFF, 0xFF, 0xFF, 'j' };
            std::lock_guard<std::mutex> lock(mutex_);
            outgoing_packets_.push({ std::move(out), srcIP, srcPort });
            return true;
        }

        return false;
    }

    int GetNextOutgoingPacket(void *pOut, int cbMaxOut, uint32 *pNetAdr, uint16 *pPort) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outgoing_packets_.empty())
            return 0;

        auto pkt = std::move(outgoing_packets_.front());
        outgoing_packets_.pop();

        if (cbMaxOut < static_cast<int>(pkt.payload.size()))
            return 0;

        memcpy(pOut, pkt.payload.data(), pkt.payload.size());
        if (pNetAdr) *pNetAdr = pkt.ip;
        if (pPort) *pPort = pkt.port;

        return static_cast<int>(pkt.payload.size());
    }

    void EnableHeartbeats(bool active) override
    {
        is_active_ = active;
        if (!active)
            map_name_.clear();
    }
    void SetHeartbeatInterval(int) override {}
    void ForceHeartbeat() override {}
    SteamAPICall_t AssociateWithClan(CSteamID) override { return k_uAPICallInvalid; }
    SteamAPICall_t ComputeNewPlayerCompatibility(CSteamID) override { return k_uAPICallInvalid; }
};

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamUtils
//-----------------------------------------------------------------------------
class SteamUtilsStub : public ISteamUtils
{
public:
    uint32 GetSecondsSinceAppActive() override { return 1; }
    uint32 GetSecondsSinceComputerActive() override { return 100; }
    EUniverse GetConnectedUniverse() override { return k_EUniversePublic; }
    uint32 GetServerRealTime() override { return static_cast<uint32>(time(nullptr)); }
    const char *GetIPCountry() override { return "US"; }
    bool GetImageSize(int, uint32 *, uint32 *) override { return false; }
    bool GetImageRGBA(int, uint8 *, int) override { return false; }
    bool GetCSERIPPort(uint32 *, uint16 *) override { return false; }
    uint8 GetCurrentBatteryPower() override { return 255; }
    uint32 GetAppID() override { return 10; } // Counter-Strike 1.6 AppID
    void SetOverlayNotificationPosition(ENotificationPosition) override {}
    bool IsAPICallCompleted(SteamAPICall_t, bool *) override { return true; }
    ESteamAPICallFailure GetAPICallFailureReason(SteamAPICall_t) override { return k_ESteamAPICallFailureNone; }
    bool GetAPICallResult(SteamAPICall_t, void *, int, int, bool *) override { return false; }
    void RunFrame() override {}
    uint32 GetIPCCallCount() override { return 0; }
    void SetWarningMessageHook(SteamAPIWarningMessageHook_t) override {}
    bool IsOverlayEnabled() override { return false; }
    bool BOverlayNeedsPresent() override { return false; }
#ifndef _PS3
    SteamAPICall_t CheckFileSignature(const char *) override { return k_uAPICallInvalid; }
#endif
    bool ShowGamepadTextInput(EGamepadTextInputMode, EGamepadTextInputLineMode, const char *, uint32) override { return false; }
    uint32 GetEnteredGamepadTextLength() override { return 0; }
    bool GetEnteredGamepadTextInput(char *, uint32) override { return false; }
    const char *GetSteamUILanguage() override { return "english"; }
};

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamMatchmaking
//-----------------------------------------------------------------------------
class SteamMatchmakingStub : public ISteamMatchmaking
{
public:
    int GetFavoriteGameCount() override { return 0; }
    bool GetFavoriteGame(int, AppId_t *, uint32 *, uint16 *, uint16 *, uint32 *, uint32 *) override { return false; }
    int AddFavoriteGame(AppId_t, uint32, uint16, uint16, uint32, uint32) override { return 0; }
    bool RemoveFavoriteGame(AppId_t, uint32, uint16, uint16, uint32) override { return false; }
    SteamAPICall_t RequestLobbyList() override { return k_uAPICallInvalid; }
    void AddRequestLobbyListStringFilter(const char *, const char *, ELobbyComparison) override {}
    void AddRequestLobbyListNumericalFilter(const char *, int, ELobbyComparison) override {}
    void AddRequestLobbyListNearValueFilter(const char *, int) override {}
    void AddRequestLobbyListFilterSlotsAvailable(int) override {}
    void AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter) override {}
    void AddRequestLobbyListResultCountFilter(int) override {}
    void AddRequestLobbyListCompatibleMembersFilter(CSteamID) override {}
    CSteamID GetLobbyByIndex(int) override { return CSteamID(); }
    SteamAPICall_t CreateLobby(ELobbyType, int) override { return k_uAPICallInvalid; }
    SteamAPICall_t JoinLobby(CSteamID) override { return k_uAPICallInvalid; }
    void LeaveLobby(CSteamID) override {}
    bool InviteUserToLobby(CSteamID, CSteamID) override { return false; }
    int GetNumLobbyMembers(CSteamID) override { return 0; }
    CSteamID GetLobbyMemberByIndex(CSteamID, int) override { return CSteamID(); }
    const char *GetLobbyData(CSteamID, const char *) override { return ""; }
    bool SetLobbyData(CSteamID, const char *, const char *) override { return false; }
    int GetLobbyDataCount(CSteamID) override { return 0; }
    bool GetLobbyDataByIndex(CSteamID, int, char *, int, char *, int) override { return false; }
    bool DeleteLobbyData(CSteamID, const char *) override { return false; }
    const char *GetLobbyMemberData(CSteamID, CSteamID, const char *) override { return ""; }
    void SetLobbyMemberData(CSteamID, const char *, const char *) override {}
    bool SendLobbyChatMsg(CSteamID, const void *, int) override { return false; }
    int GetLobbyChatEntry(CSteamID, int, CSteamID *, void *, int, EChatEntryType *) override { return 0; }
    bool RequestLobbyData(CSteamID) override { return false; }
    void SetLobbyGameServer(CSteamID, uint32, uint16, CSteamID) override {}
    bool GetLobbyGameServer(CSteamID, uint32 *, uint16 *, CSteamID *) override { return false; }
    bool SetLobbyMemberLimit(CSteamID, int) override { return false; }
    int GetLobbyMemberLimit(CSteamID) override { return 0; }
    bool SetLobbyType(CSteamID, ELobbyType) override { return false; }
    bool SetLobbyJoinable(CSteamID, bool) override { return false; }
    CSteamID GetLobbyOwner(CSteamID) override { return CSteamID(); }
    bool SetLobbyOwner(CSteamID, CSteamID) override { return false; }
    bool SetLinkedLobby(CSteamID, CSteamID) override { return false; }
#ifdef _PS3
    void CheckForPSNGameBootInvite(unsigned int) override {}
#endif
};

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamMatchmakingServers
//-----------------------------------------------------------------------------
class SteamMatchmakingServersStub : public ISteamMatchmakingServers
{
public:
    HServerListRequest RequestInternetServerList(AppId_t, MatchMakingKeyValuePair_t **, uint32, ISteamMatchmakingServerListResponse *) override { return 0; }
    HServerListRequest RequestLANServerList(AppId_t, ISteamMatchmakingServerListResponse *pResponse) override {
        if (pResponse)
            pResponse->RefreshComplete(0, eServerResponded);
        return 0;
    }
    HServerListRequest RequestFriendsServerList(AppId_t, MatchMakingKeyValuePair_t **, uint32, ISteamMatchmakingServerListResponse *) override { return 0; }
    HServerListRequest RequestFavoritesServerList(AppId_t, MatchMakingKeyValuePair_t **, uint32, ISteamMatchmakingServerListResponse *) override { return 0; }
    HServerListRequest RequestHistoryServerList(AppId_t, MatchMakingKeyValuePair_t **, uint32, ISteamMatchmakingServerListResponse *) override { return 0; }
    HServerListRequest RequestSpectatorServerList(AppId_t, MatchMakingKeyValuePair_t **, uint32, ISteamMatchmakingServerListResponse *) override { return 0; }
    void ReleaseRequest(HServerListRequest) override {}
    gameserveritem_t *GetServerDetails(HServerListRequest, int) override;
    void CancelQuery(HServerListRequest) override {}
    void RefreshQuery(HServerListRequest) override {}
    bool IsRefreshing(HServerListRequest) override { return false; }
    int GetServerCount(HServerListRequest) override { return 0; }
    void RefreshServer(HServerListRequest, int) override {}
    HServerQuery PingServer(uint32, uint16, ISteamMatchmakingPingResponse *) override { return 0; }
    HServerQuery PlayerDetails(uint32, uint16, ISteamMatchmakingPlayersResponse *) override { return 0; }
    HServerQuery ServerRules(uint32, uint16, ISteamMatchmakingRulesResponse *) override { return 0; }
    void CancelServerQuery(HServerQuery) override {}
};

//-----------------------------------------------------------------------------
// Safe crash-proof stub for ISteamHTTP
//-----------------------------------------------------------------------------
class SteamHTTPStub : public ISteamHTTP
{
public:
    HTTPRequestHandle CreateHTTPRequest(EHTTPMethod, const char *) override { return INVALID_HTTPREQUEST_HANDLE; }
    bool SetHTTPRequestContextValue(HTTPRequestHandle, uint64) override { return false; }
    bool SetHTTPRequestNetworkActivityTimeout(HTTPRequestHandle, uint32) override { return false; }
    bool SetHTTPRequestHeaderValue(HTTPRequestHandle, const char *, const char *) override { return false; }
    bool SetHTTPRequestGetOrPostParameter(HTTPRequestHandle, const char *, const char *) override { return false; }
    bool SendHTTPRequest(HTTPRequestHandle, SteamAPICall_t *) override { return false; }
    bool SendHTTPRequestAndStreamResponse(HTTPRequestHandle, SteamAPICall_t *) override { return false; }
    bool DeferHTTPRequest(HTTPRequestHandle) override { return false; }
    bool PrioritizeHTTPRequest(HTTPRequestHandle) override { return false; }
    bool GetHTTPResponseHeaderSize(HTTPRequestHandle, const char *, uint32 *) override { return false; }
    bool GetHTTPResponseHeaderValue(HTTPRequestHandle, const char *, uint8 *, uint32) override { return false; }
    bool GetHTTPResponseBodySize(HTTPRequestHandle, uint32 *) override { return false; }
    bool GetHTTPResponseBodyData(HTTPRequestHandle, uint8 *, uint32) override { return false; }
    bool GetHTTPStreamingResponseBodyData(HTTPRequestHandle, uint32, uint8 *, uint32) override { return false; }
    bool ReleaseHTTPRequest(HTTPRequestHandle) override { return true; }
    bool GetHTTPDownloadProgressPct(HTTPRequestHandle, float *) override { return false; }
    bool SetHTTPRequestRawPostBody(HTTPRequestHandle, const char *, uint8 *, uint32) override { return false; }
};

extern SteamFriendsStub g_SteamFriendsStub;
extern SteamAppsStub g_SteamAppsStub;
extern SteamGameServerStub g_SteamGameServerStub;
extern SteamUtilsStub g_SteamUtilsStub;
extern SteamMatchmakingStub g_SteamMatchmakingStub;
extern SteamMatchmakingServersStub g_SteamMatchmakingServersStub;
extern SteamHTTPStub g_SteamHTTPStub;

