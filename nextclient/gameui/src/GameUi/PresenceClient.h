#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

struct PlayerPresenceInfo
{
    std::string name;
    std::string state; // "ingame" or "lobby"
    std::string map;
    std::string server;
    std::string edition;
    std::string tag;
    int durationSec = 0;
    int idleSec = 0;
};

struct LobbyChatMessage
{
    int id = 0;
    std::string sender;
    std::string deviceHash;
    std::string edition;
    std::string tag;
    std::string text;
    std::string time;
};

class PresenceClient
{
public:
    static PresenceClient& GetInstance();

    void Initialize();
    void Shutdown();
    void UpdateMainThreadState();

    // Fast atomic getters for HUD / Menu rendering (zero overhead)
    int GetTotalOnline() const { return m_totalOnline.load(); }
    int GetInLobby() const { return m_inLobby.load(); }
    int GetInGame() const { return m_inGame.load(); }
    bool HasData() const { return m_hasData.load(); }

    // Detailed players list (Asynchronously fetched on demand)
    void FetchPlayersListAsync(std::function<void(bool success, const std::vector<PlayerPresenceInfo>& players)> callback);
    std::vector<PlayerPresenceInfo> GetCachedPlayers();

    // Global Lobby Chat methods
    void FetchChatMessagesAsync(int sinceId, std::function<void(bool success, int lastId, const std::vector<LobbyChatMessage>& messages)> callback);
    void SendChatMessageAsync(const std::string& message, std::function<void(bool success, const std::string& errorOrNotice, const LobbyChatMessage& sentMsg)> callback);
    bool GetLatestChatMessage(LobbyChatMessage& outMsg);
    void UpdateLatestChatMessage(const LobbyChatMessage& msg);

    // Helpers
    std::string GetDeviceHash();
    std::string GetCurrentPlayerName();
    bool IsClientInGame();
    std::string GetCurrentMap();
    std::string GetCurrentServer();
    std::string GetClientTag();
    std::string GetClientEdition();

private:
    PresenceClient();
    ~PresenceClient();
    PresenceClient(const PresenceClient&) = delete;
    PresenceClient& operator=(const PresenceClient&) = delete;

    void WorkerLoop();
    void SendHeartbeat();
    void SendLeaveNotification();

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    std::atomic<int> m_totalOnline{0};
    std::atomic<int> m_inLobby{0};
    std::atomic<int> m_inGame{0};
    std::atomic<bool> m_hasData{false};

    std::mutex m_playersMutex;
    std::vector<PlayerPresenceInfo> m_cachedPlayers;

    std::mutex m_stateMutex;
    bool m_bLocalInGame{false};
    std::string m_currentPlayerName{"Player"};
    std::string m_currentMap;
    std::string m_currentServer;

    std::string m_cachedDeviceHash;
    std::string m_cachedEdition;
    std::string m_cachedTag;

    std::mutex m_latestChatMutex;
    LobbyChatMessage m_latestChatMessage;
    bool m_hasLatestChat{false};
};
