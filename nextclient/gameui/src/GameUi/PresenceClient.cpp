#include "PresenceClient.h"
#include "GameUi.h"
#include <windows.h>
#include <cpr/cpr.h>
#include <sstream>
#include <algorithm>
#include <chrono>

#ifndef NEXTCLIENT_TAG
#define NEXTCLIENT_TAG "DEFAULT"
#endif

namespace
{
constexpr char kPrimaryHost[] = "http://gameland.cam";
constexpr char kFallbackHost[] = "http://185.161.112.56";
constexpr char kHostHeader[] = "gameland.cam";

std::string ExtractJsonField(const std::string& json, const std::string& key)
{
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos)
        return "";
    pos += pattern.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
        pos++;
    if (pos >= json.size())
        return "";

    if (json[pos] == '"')
    {
        pos++;
        std::string result;
        bool escaping = false;
        while (pos < json.size())
        {
            char c = json[pos];
            if (escaping)
            {
                if (c == 'u' && (pos + 4 < json.size()))
                {
                    std::string hexStr = json.substr(pos + 1, 4);
                    try {
                        unsigned int codepoint = (unsigned int)std::stoul(hexStr, nullptr, 16);
                        if (codepoint <= 0x7F) {
                            result += (char)codepoint;
                        } else if (codepoint <= 0x7FF) {
                            result += (char)(0xC0 | ((codepoint >> 6) & 0x1F));
                            result += (char)(0x80 | (codepoint & 0x3F));
                        } else {
                            result += (char)(0xE0 | ((codepoint >> 12) & 0x0F));
                            result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            result += (char)(0x80 | (codepoint & 0x3F));
                        }
                        pos += 4;
                    } catch (...) {
                        result += c;
                    }
                }
                else if (c == 'n') result += '\n';
                else if (c == 'r') result += '\r';
                else if (c == 't') result += '\t';
                else result += c;
                escaping = false;
            }
            else if (c == '\\')
            {
                escaping = true;
            }
            else if (c == '"')
            {
                break;
            }
            else
            {
                result += c;
            }
            pos++;
        }
        return result;
    }
    else
    {
        size_t end = json.find_first_of(",}\r\n \t", pos);
        if (end == std::string::npos)
            end = json.size();
        return json.substr(pos, end - pos);
    }
}

int ExtractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0)
{
    std::string val = ExtractJsonField(json, key);
    if (val.empty()) return defaultVal;
    try {
        return std::stoi(val);
    } catch (...) {
        return defaultVal;
    }
}

cpr::Response SafeHttpPost(const std::string& path, const cpr::Payload& payload, int timeoutMs = 3500)
{
    std::string url = std::string(kPrimaryHost) + path;
    cpr::Response r = cpr::Post(
        cpr::Url{url},
        payload,
        cpr::Timeout{timeoutMs}
    );

    // Fallback directly to host IP with Host header if DNS resolution or connection failed
    if (r.status_code == 0 || r.error.code != cpr::ErrorCode::OK)
    {
        std::string fallbackUrl = std::string(kFallbackHost) + path;
        r = cpr::Post(
            cpr::Url{fallbackUrl},
            payload,
            cpr::Header{{"Host", kHostHeader}},
            cpr::Timeout{timeoutMs}
        );
    }
    return r;
}

cpr::Response SafeHttpGet(const std::string& path, int timeoutMs = 3500)
{
    std::string url = std::string(kPrimaryHost) + path;
    cpr::Response r = cpr::Get(
        cpr::Url{url},
        cpr::Timeout{timeoutMs}
    );

    if (r.status_code == 0 || r.error.code != cpr::ErrorCode::OK)
    {
        std::string fallbackUrl = std::string(kFallbackHost) + path;
        r = cpr::Get(
            cpr::Url{fallbackUrl},
            cpr::Header{{"Host", kHostHeader}},
            cpr::Timeout{timeoutMs}
        );
    }
    return r;
}

std::string ComputeLocal24DeviceHash()
{
    std::string machineGuid;
    char guidBuf[128] = {0};
    DWORD guidSize = sizeof(guidBuf);
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(guidBuf), &guidSize) == ERROR_SUCCESS)
            machineGuid = guidBuf;
        RegCloseKey(hKey);
    }
    if (machineGuid.empty())
    {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS)
        {
            guidSize = sizeof(guidBuf);
            if (RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(guidBuf), &guidSize) == ERROR_SUCCESS)
                machineGuid = guidBuf;
            RegCloseKey(hKey);
        }
    }

    DWORD volSerial = 0, maxLen = 0, flags = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &volSerial, &maxLen, &flags, nullptr, 0);
    char volBuf[32] = {0};
    sprintf_s(volBuf, "%08X", volSerial);

    char compBuf[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD compLen = sizeof(compBuf);
    GetComputerNameA(compBuf, &compLen);

    std::string seed = machineGuid + "_" + volBuf + "_" + compBuf;
    if (seed.length() < 10) seed = "GAMELAND_STATIC_HWID_SEED_2026";

    // FNV-1a 64-bit hash combined
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0x100000001b3ULL;
    for (char c : seed)
    {
        h1 = (h1 ^ static_cast<uint8_t>(c)) * 0x100000001b3ULL;
        h2 = (h2 ^ static_cast<uint8_t>(c)) * 0xcbf29ce484222325ULL;
    }

    char outBuf[32] = {0};
    sprintf_s(outBuf, "%012llX%012llX", (unsigned long long)(h1 & 0xFFFFFFFFFFFFULL), (unsigned long long)(h2 & 0xFFFFFFFFFFFFULL));
    return std::string(outBuf);
}
} // namespace

PresenceClient& PresenceClient::GetInstance()
{
    static PresenceClient s_instance;
    return s_instance;
}

PresenceClient::PresenceClient()
{
    m_cachedDeviceHash = ComputeLocal24DeviceHash();

#if defined(GAMELAND_HOME_CLIENT) && GAMELAND_HOME_CLIENT
    m_cachedEdition = "home";
    m_cachedTag = "HOME";
#else
    m_cachedEdition = "gamenet";
    m_cachedTag = NEXTCLIENT_TAG;
#endif
}

PresenceClient::~PresenceClient()
{
    Shutdown();
}

void PresenceClient::Initialize()
{
    if (m_running.load())
        return;

    m_running.store(true);
    m_workerThread = std::thread(&PresenceClient::WorkerLoop, this);
}

void PresenceClient::Shutdown()
{
    if (!m_running.load())
        return;

    m_running.store(false);

    SendLeaveNotification();

    if (m_workerThread.joinable())
    {
        m_workerThread.join();
    }
}

void PresenceClient::UpdateMainThreadState()
{
    if (!engine)
        return;

    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_bLocalInGame = GameUI().IsInLevel();

    const char* pName = engine->pfnGetCvarString("name");
    m_currentPlayerName = (pName && *pName) ? pName : "Player";

    if (m_bLocalInGame)
    {
        const char* pMap = engine->pfnGetLevelName();
        m_currentMap = (pMap && *pMap) ? pMap : "";

        if (GameClientExports())
        {
            const char* pHost = GameClientExports()->GetServerHostName();
            m_currentServer = (pHost && *pHost) ? pHost : "";
        }
    }
    else
    {
        m_currentMap.clear();
        m_currentServer.clear();
    }
}

std::string PresenceClient::GetDeviceHash()
{
    return m_cachedDeviceHash;
}

std::string PresenceClient::GetCurrentPlayerName()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_currentPlayerName;
}

bool PresenceClient::IsClientInGame()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_bLocalInGame;
}

std::string PresenceClient::GetCurrentMap()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_currentMap;
}

std::string PresenceClient::GetCurrentServer()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_currentServer;
}

std::string PresenceClient::GetClientTag()
{
    return m_cachedTag;
}

std::string PresenceClient::GetClientEdition()
{
    return m_cachedEdition;
}

void PresenceClient::SendHeartbeat()
{
    std::string name, state, map, server;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        name = m_currentPlayerName.empty() ? "Player" : m_currentPlayerName;
        state = m_bLocalInGame ? "ingame" : "lobby";
        map = m_currentMap;
        server = m_currentServer;
    }

    cpr::Payload payload{
        {"action", "heartbeat"},
        {"device_hash", m_cachedDeviceHash},
        {"name", name},
        {"state", state},
        {"map", map},
        {"server", server},
        {"edition", m_cachedEdition},
        {"tag", m_cachedTag}
    };

    SafeHttpPost("/presence.php", payload);

    // Fetch refreshed summary
    cpr::Response sumResp = SafeHttpGet("/presence.php?action=summary");
    if (sumResp.status_code == 200 && !sumResp.text.empty())
    {
        int total = ExtractJsonInt(sumResp.text, "total_online", 0);
        int lobby = ExtractJsonInt(sumResp.text, "in_lobby", 0);
        int game = ExtractJsonInt(sumResp.text, "in_game", 0);

        m_totalOnline.store(total);
        m_inLobby.store(lobby);
        m_inGame.store(game);
        m_hasData.store(true);
    }

    // Fetch latest lobby chat message for the right-hand translucent HUD widget
    cpr::Response chatResp = SafeHttpGet("/chat.php?action=get_latest", 2500);
    if (chatResp.status_code == 200 && !chatResp.text.empty())
    {
        size_t msgPos = chatResp.text.find("\"message\":{");
        if (msgPos != std::string::npos)
        {
            size_t objStart = msgPos + 10;
            size_t objEnd = chatResp.text.find('}', objStart);
            if (objEnd != std::string::npos)
            {
                std::string itemJson = chatResp.text.substr(objStart, objEnd - objStart + 1);
                LobbyChatMessage msg;
                msg.id = ExtractJsonInt(itemJson, "id", 0);
                msg.sender = ExtractJsonField(itemJson, "sender");
                msg.deviceHash = ExtractJsonField(itemJson, "device_hash");
                msg.edition = ExtractJsonField(itemJson, "edition");
                msg.tag = ExtractJsonField(itemJson, "tag");
                msg.text = ExtractJsonField(itemJson, "text");
                msg.time = ExtractJsonField(itemJson, "time");

                if (!msg.text.empty() && msg.id > 0)
                {
                    UpdateLatestChatMessage(msg);
                }
            }
        }
    }
}

void PresenceClient::SendLeaveNotification()
{
    cpr::Payload payload{
        {"action", "leave"},
        {"device_hash", m_cachedDeviceHash}
    };
    SafeHttpPost("/presence.php", payload, 1000);
}

void PresenceClient::WorkerLoop()
{
    // Sleep initially so engine and UI finish initializing cleanly
    for (int i = 0; i < 20 && m_running.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!m_running.load())
        return;

    SendHeartbeat();

    while (m_running.load())
    {
        bool inGame = false;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            inGame = m_bLocalInGame;
        }

        int waitSeconds = inGame ? 45 : 20;
        for (int i = 0; i < waitSeconds * 10 && m_running.load(); ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!m_running.load())
            break;

        SendHeartbeat();
    }
}

void PresenceClient::FetchPlayersListAsync(std::function<void(bool success, const std::vector<PlayerPresenceInfo>& players)> callback)
{
    std::thread([this, callback]() {
        cpr::Response r = SafeHttpGet("/presence.php?action=players", 4000);
        if (r.status_code != 200 || r.text.empty())
        {
            if (callback) callback(false, {});
            return;
        }

        std::vector<PlayerPresenceInfo> players;
        const std::string& json = r.text;

        size_t playersPos = json.find("\"players\":[");
        if (playersPos != std::string::npos)
        {
            playersPos += 11;
            while (playersPos < json.size())
            {
                size_t objStart = json.find('{', playersPos);
                if (objStart == std::string::npos) break;
                size_t objEnd = json.find('}', objStart);
                if (objEnd == std::string::npos) break;

                std::string itemJson = json.substr(objStart, objEnd - objStart + 1);

                PlayerPresenceInfo info;
                info.name = ExtractJsonField(itemJson, "name");
                info.state = ExtractJsonField(itemJson, "state");
                info.map = ExtractJsonField(itemJson, "map");
                info.server = ExtractJsonField(itemJson, "server");
                info.edition = ExtractJsonField(itemJson, "edition");
                info.tag = ExtractJsonField(itemJson, "tag");
                info.durationSec = ExtractJsonInt(itemJson, "duration", 0);
                info.idleSec = ExtractJsonInt(itemJson, "idle", 0);

                if (!info.name.empty())
                    players.push_back(info);

                playersPos = objEnd + 1;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_playersMutex);
            m_cachedPlayers = players;
        }

        if (callback)
            callback(true, players);
    }).detach();
}

std::vector<PlayerPresenceInfo> PresenceClient::GetCachedPlayers()
{
    std::lock_guard<std::mutex> lock(m_playersMutex);
    return m_cachedPlayers;
}

void PresenceClient::FetchChatMessagesAsync(int sinceId, std::function<void(bool success, int lastId, const std::vector<LobbyChatMessage>& messages)> callback)
{
    std::thread([sinceId, callback]() {
        std::string path = "/chat.php?action=get_messages&since_id=" + std::to_string(sinceId);
        cpr::Response r = SafeHttpGet(path, 3000);

        if (r.status_code != 200 || r.text.empty())
        {
            if (callback) callback(false, sinceId, {});
            return;
        }

        int lastId = ExtractJsonInt(r.text, "last_id", sinceId);
        std::vector<LobbyChatMessage> messages;
        const std::string& json = r.text;

        size_t listPos = json.find("\"messages\":[");
        if (listPos != std::string::npos)
        {
            listPos += 12;
            while (listPos < json.size())
            {
                size_t objStart = json.find('{', listPos);
                if (objStart == std::string::npos) break;

                // Safely match matching closing brace taking string literals & escapes into account
                size_t objEnd = std::string::npos;
                bool inStr = false;
                bool esc = false;
                int depth = 0;
                for (size_t p = objStart; p < json.size(); ++p)
                {
                    char c = json[p];
                    if (esc) { esc = false; continue; }
                    if (c == '\\') { if (inStr) esc = true; continue; }
                    if (c == '"') { inStr = !inStr; continue; }
                    if (!inStr)
                    {
                        if (c == '{') depth++;
                        else if (c == '}')
                        {
                            depth--;
                            if (depth == 0) { objEnd = p; break; }
                        }
                    }
                }

                if (objEnd == std::string::npos) break;

                std::string itemJson = json.substr(objStart, objEnd - objStart + 1);

                LobbyChatMessage msg;
                msg.id = ExtractJsonInt(itemJson, "id", 0);
                msg.sender = ExtractJsonField(itemJson, "sender");
                msg.deviceHash = ExtractJsonField(itemJson, "device_hash");
                msg.edition = ExtractJsonField(itemJson, "edition");
                msg.tag = ExtractJsonField(itemJson, "tag");
                msg.text = ExtractJsonField(itemJson, "text");
                msg.time = ExtractJsonField(itemJson, "time");

                if (!msg.text.empty() && msg.id > 0)
                    messages.push_back(msg);

                listPos = objEnd + 1;
            }
        }

        if (!messages.empty())
        {
            PresenceClient::GetInstance().UpdateLatestChatMessage(messages.back());
        }

        if (callback)
            callback(true, lastId, messages);
    }).detach();
}

void PresenceClient::SendChatMessageAsync(const std::string& message, std::function<void(bool success, const std::string& errorOrNotice, const LobbyChatMessage& sentMsg)> callback)
{
    std::string devHash = m_cachedDeviceHash;
    std::string name = GetCurrentPlayerName();
    std::string edition = m_cachedEdition;
    std::string tag = m_cachedTag;

    std::thread([devHash, name, edition, tag, message, callback]() {
        cpr::Payload payload{
            {"action", "send_message"},
            {"device_hash", devHash},
            {"name", name},
            {"edition", edition},
            {"tag", tag},
            {"message", message}
        };

        cpr::Response r = SafeHttpPost("/chat.php", payload, 3500);
        if (r.status_code != 200 || r.text.empty())
        {
            if (callback) callback(false, "Connection error to chat host.", {});
            return;
        }

        std::string success = ExtractJsonField(r.text, "success");
        if (success == "true")
        {
            LobbyChatMessage sentMsg;
            size_t msgPos = r.text.find("\"message\":{");
            if (msgPos != std::string::npos)
            {
                size_t objStart = msgPos + 10;
                size_t objEnd = r.text.find('}', objStart);
                if (objEnd != std::string::npos)
                {
                    std::string itemJson = r.text.substr(objStart, objEnd - objStart + 1);
                    sentMsg.id = ExtractJsonInt(itemJson, "id", 0);
                    sentMsg.sender = ExtractJsonField(itemJson, "sender");
                    sentMsg.deviceHash = ExtractJsonField(itemJson, "device_hash");
                    sentMsg.edition = ExtractJsonField(itemJson, "edition");
                    sentMsg.tag = ExtractJsonField(itemJson, "tag");
                    sentMsg.text = ExtractJsonField(itemJson, "text");
                    sentMsg.time = ExtractJsonField(itemJson, "time");
                }
            }
            if (sentMsg.id > 0)
            {
                PresenceClient::GetInstance().UpdateLatestChatMessage(sentMsg);
            }
            if (callback) callback(true, "", sentMsg);
        }
        else
        {
            std::string err = ExtractJsonField(r.text, "error");
            if (err.empty()) err = "Failed to send message.";
            if (callback) callback(false, err, {});
        }
    }).detach();
}

bool PresenceClient::GetLatestChatMessage(LobbyChatMessage& outMsg)
{
    std::lock_guard<std::mutex> lock(m_latestChatMutex);
    if (m_hasLatestChat)
    {
        outMsg = m_latestChatMessage;
        return true;
    }
    return false;
}

void PresenceClient::UpdateLatestChatMessage(const LobbyChatMessage& msg)
{
    std::lock_guard<std::mutex> lock(m_latestChatMutex);
    m_latestChatMessage = msg;
    m_hasLatestChat = true;
}
