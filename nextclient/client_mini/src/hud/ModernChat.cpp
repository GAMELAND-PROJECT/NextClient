#include "ModernChat.h"
#include "../main.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>

namespace
{
    void DrawBox(int x, int y, int w, int h, int r, int g, int b, int a)
    {
        if (gEngfuncs.pfnFillRGBA != nullptr && w > 0 && h > 0 && a > 0)
            gEngfuncs.pfnFillRGBA(x, y, w, h, r, g, b, a);
    }

    void DrawTextWithShadow(int x, int y, const char* str, float r, float g, float b)
    {
        if (str == nullptr || *str == 0)
            return;

        // 1. Soft Obsidian Drop Shadow (1px offset) for crystal-clear readability on all maps
        if (gEngfuncs.pfnDrawSetTextColor != nullptr)
            gEngfuncs.pfnDrawSetTextColor(0.04f, 0.05f, 0.08f);
        if (gEngfuncs.pfnDrawConsoleString != nullptr)
            gEngfuncs.pfnDrawConsoleString(x + 1, y + 1, const_cast<char*>(str));

        // 2. Crisp Sharp Foreground Text
        if (gEngfuncs.pfnDrawSetTextColor != nullptr)
            gEngfuncs.pfnDrawSetTextColor(r, g, b);
        if (gEngfuncs.pfnDrawConsoleString != nullptr)
            gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(str));
    }

    void GetTextSize(const char* str, int& outW, int& outH)
    {
        outW = 0;
        outH = 14;
        if (gEngfuncs.pfnDrawConsoleStringLen != nullptr && str != nullptr && *str != 0)
        {
            gEngfuncs.pfnDrawConsoleStringLen(const_cast<char*>(str), &outW, &outH);
        }
    }

    void PopUtf8Char(std::string& str)
    {
        if (str.empty())
            return;
        if ((static_cast<unsigned char>(str.back()) & 0x80) == 0)
        {
            str.pop_back();
            return;
        }
        while (!str.empty() && (static_cast<unsigned char>(str.back()) & 0xC0) == 0x80)
        {
            str.pop_back();
        }
        if (!str.empty())
        {
            str.pop_back();
        }
    }

    std::string StripColorCodes(const std::string& input)
    {
        std::string result;
        result.reserve(input.size());
        for (char c : input)
        {
            unsigned char uc = static_cast<unsigned char>(c);
            // Filter non-printable control characters, including CS \x01-\x04 and newlines
            if (uc < 32 && uc != '\t')
                continue;
            result.push_back(c);
        }
        return result;
    }

#ifdef _WIN32
    void PasteClipboardToChat(std::string& buffer, size_t maxLen = 120)
    {
        if (!OpenClipboard(nullptr))
            return;
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData != nullptr)
        {
            const char* pszText = static_cast<const char*>(GlobalLock(hData));
            if (pszText != nullptr)
            {
                for (const char* p = pszText; *p != 0 && buffer.size() < maxLen; ++p)
                {
                    if (*p == '\r' || *p == '\n') break;
                    if (static_cast<unsigned char>(*p) >= 32)
                    {
                        buffer.push_back(*p);
                    }
                }
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
#endif
    bool StartsWithCI(std::string_view str, std::string_view prefix)
    {
        if (str.length() < prefix.length())
            return false;
        for (size_t i = 0; i < prefix.length(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(str[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        }
        return true;
    }

    bool EndsWithCI(std::string_view str, std::string_view suffix)
    {
        if (str.length() < suffix.length())
            return false;
        size_t offset = str.length() - suffix.length();
        for (size_t i = 0; i < suffix.length(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(str[offset + i])) !=
                std::tolower(static_cast<unsigned char>(suffix[i])))
                return false;
        }
        return true;
    }

    bool EqualsCI(std::string_view a, std::string_view b)
    {
        if (a.length() != b.length())
            return false;
        for (size_t i = 0; i < a.length(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }

    void TrimString(std::string& str)
    {
        while (!str.empty() && (static_cast<unsigned char>(str.front()) <= 32 || str.front() == '\t'))
            str.erase(0, 1);
        while (!str.empty() && (static_cast<unsigned char>(str.back()) <= 32 || str.back() == '\t'))
            str.pop_back();
    }

    void CleanChatMessage(std::string& sender, std::string& msg, const std::string& realPlayerName = "")
    {
        TrimString(sender);
        TrimString(msg);

        // Strip trailing colons and spaces from sender
        while (!sender.empty() && (sender.back() == ':' || static_cast<unsigned char>(sender.back()) <= 32))
            sender.pop_back();
        TrimString(sender);

        // Strip known status prefixes from msg
        auto stripKnownPrefix = [](std::string& str, std::string_view pfx)
        {
            if (StartsWithCI(str, pfx))
            {
                str.erase(0, pfx.length());
                TrimString(str);
            }
        };

        stripKnownPrefix(msg, "*DEAD*");
        stripKnownPrefix(msg, "(DEAD)");
        stripKnownPrefix(msg, "[DEAD]");
        stripKnownPrefix(msg, "(Terrorist)");
        stripKnownPrefix(msg, "[Terrorist]");
        stripKnownPrefix(msg, "(Counter-Terrorist)");
        stripKnownPrefix(msg, "[Counter-Terrorist]");
        stripKnownPrefix(msg, "(Spectator)");
        stripKnownPrefix(msg, "[Spectator]");
        stripKnownPrefix(msg, "(ALL)");
        stripKnownPrefix(msg, "[ALL]");

        // Strip known status prefixes from sender as well if duplicated
        stripKnownPrefix(sender, "*DEAD*");
        stripKnownPrefix(sender, "(DEAD)");
        stripKnownPrefix(sender, "[DEAD]");
        stripKnownPrefix(sender, "(Terrorist)");
        stripKnownPrefix(sender, "[Terrorist]");
        stripKnownPrefix(sender, "(Counter-Terrorist)");
        stripKnownPrefix(sender, "[Counter-Terrorist]");
        stripKnownPrefix(sender, "(Spectator)");
        stripKnownPrefix(sender, "[Spectator]");
        stripKnownPrefix(sender, "(ALL)");
        stripKnownPrefix(sender, "[ALL]");
        TrimString(sender);

        // Check if msg starts with a sender name / header followed by a colon
        // Look for the first colon in msg
        size_t colonPos = msg.find(':');
        if (colonPos != std::string::npos && colonPos < 64)
        {
            std::string header = msg.substr(0, colonPos);
            TrimString(header);

            bool isSenderHeader = false;

            if (!sender.empty())
            {
                if (EqualsCI(header, sender) ||
                    EndsWithCI(header, sender) ||
                    StartsWithCI(header, sender))
                {
                    isSenderHeader = true;
                }
            }

            if (!isSenderHeader && !realPlayerName.empty())
            {
                if (EqualsCI(header, realPlayerName) ||
                    EndsWithCI(header, realPlayerName) ||
                    StartsWithCI(header, realPlayerName))
                {
                    isSenderHeader = true;
                }
            }

            // Also check if header contains sender or realPlayerName as a substring
            if (!isSenderHeader && !sender.empty())
            {
                auto it = std::search(
                    header.begin(), header.end(),
                    sender.begin(), sender.end(),
                    [](char ch1, char ch2) {
                        return std::tolower(static_cast<unsigned char>(ch1)) ==
                               std::tolower(static_cast<unsigned char>(ch2));
                    });
                if (it != header.end())
                {
                    isSenderHeader = true;
                }
            }
            if (!isSenderHeader && !realPlayerName.empty())
            {
                auto it = std::search(
                    header.begin(), header.end(),
                    realPlayerName.begin(), realPlayerName.end(),
                    [](char ch1, char ch2) {
                        return std::tolower(static_cast<unsigned char>(ch1)) ==
                               std::tolower(static_cast<unsigned char>(ch2));
                    });
                if (it != header.end())
                {
                    isSenderHeader = true;
                }
            }

            if (isSenderHeader)
            {
                size_t afterColon = colonPos + 1;
                while (afterColon < msg.length() && (msg[afterColon] == ' ' || msg[afterColon] == '\t' || msg[afterColon] == ':'))
                    afterColon++;

                msg.erase(0, afterColon);
                TrimString(msg);
            }
        }

        // If msg literally equals sender or realPlayerName, clear it to avoid duplication
        if (!sender.empty() && EqualsCI(msg, sender))
        {
            msg.clear();
        }
        else if (!realPlayerName.empty() && EqualsCI(msg, realPlayerName))
        {
            msg.clear();
        }

        // Strip any remaining leading colons from msg
        while (!msg.empty() && (msg.front() == ':' || static_cast<unsigned char>(msg.front()) <= 32))
            msg.erase(0, 1);
        TrimString(msg);
    }
}

ModernChat& ModernChat::Instance()
{
    static ModernChat s_instance;
    return s_instance;
}

void ModernChat::Init(nitroapi::NitroApiInterface* /*nitro_api*/)
{
    m_mode = ModernChatMode::Closed;
    m_buffer.clear();
    m_openTime = 0.0;
    m_messages.clear();
    m_lastLocalSentText.clear();
    m_lastLocalSentTime = 0.0;
}

void ModernChat::VidInit()
{
    if (IsOpen())
        Cancel();
}

void ModernChat::Reset()
{
    if (IsOpen())
        Cancel();
    m_messages.clear();
    m_lastLocalSentText.clear();
    m_lastLocalSentTime = 0.0;
}

void ModernChat::Open(ModernChatMode mode)
{
    if (m_mode != ModernChatMode::Closed)
    {
        m_mode = mode;
        return;
    }

    m_mode = mode;
    m_buffer.clear();
    m_openTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);

    // NOTE: Mouse look/turning remains 100% ACTIVE!
    // We intentionally NEVER call IN_DeactivateMouse() so the player can freely aim & look 360 degrees while typing.
}

void ModernChat::Close()
{
    if (m_mode == ModernChatMode::Closed)
        return;

    m_mode = ModernChatMode::Closed;
    m_buffer.clear();
}

void ModernChat::Send()
{
    if (!m_buffer.empty())
    {
        std::string escaped;
        escaped.reserve(m_buffer.size() + 8);
        for (char c : m_buffer)
        {
            if (c == '"' || c == ';')
                continue;
            escaped.push_back(c);
        }

        if (!escaped.empty())
        {
            char cmd[320]{};
            if (m_mode == ModernChatMode::SayTeam)
                std::snprintf(cmd, sizeof(cmd), "say_team \"%s\"\n", escaped.c_str());
            else
                std::snprintf(cmd, sizeof(cmd), "say \"%s\"\n", escaped.c_str());

            m_lastLocalSentText = escaped;
            m_lastLocalSentTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);

            if (gEngfuncs.pfnClientCmd != nullptr)
                gEngfuncs.pfnClientCmd(cmd);

            // Add to live modern chat feed immediately:
            // Slash commands (e.g. /rs, /top15) route directly to the lower black card tier!
            std::string_view sv = escaped;
            while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
                sv.remove_prefix(1);

            if (sv.starts_with("/"))
            {
                const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "");
                std::string sender = (myName != nullptr && *myName != 0) ? myName : "Me";
                AddChatMessage(0, "[CMD]", sender, escaped, 0.85f, 0.88f, 0.92f, false, false, true);
            }
            else
            {
                OnLocalPlayerSend(m_mode, escaped);
            }
        }
    }

    Close();
}

void ModernChat::Cancel()
{
    Close();
}

void ModernChat::ToggleMode()
{
    if (m_mode == ModernChatMode::SayAll)
        m_mode = ModernChatMode::SayTeam;
    else if (m_mode == ModernChatMode::SayTeam)
        m_mode = ModernChatMode::SayAll;
}

void ModernChat::AddChatMessage(int clientIndex, const std::string& prefix, const std::string& sender, const std::string& text, float r, float g, float b, bool isTeam, bool isServer, bool isCommand)
{
    std::string cleanText = StripColorCodes(text);
    std::string cleanSender = StripColorCodes(sender);
    std::string cleanPrefix = StripColorCodes(prefix);

    TrimString(cleanPrefix);
    TrimString(cleanSender);
    TrimString(cleanText);

    if (cleanText.empty())
        return;

    std::string realPlayerName;
    if (clientIndex >= 1 && clientIndex <= 32 && gEngfuncs.pfnGetPlayerInfo != nullptr)
    {
        hud_player_info_t pinfo{};
        gEngfuncs.pfnGetPlayerInfo(clientIndex, &pinfo);
        if (pinfo.name != nullptr && pinfo.name[0] != '\0')
        {
            realPlayerName = StripColorCodes(pinfo.name);
            TrimString(realPlayerName);
        }
    }

    if (!isServer && !isCommand)
    {
        CleanChatMessage(cleanSender, cleanText, realPlayerName);

        // Fallback if sender became empty
        if (cleanSender.empty())
        {
            if (!realPlayerName.empty())
                cleanSender = realPlayerName;
            else if (isTeam)
                cleanSender = "Teammate";
            else
                cleanSender = "Player";
        }
    }

    const double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);

    // Robust deduplication: prevent double messages from SayText + TextMsg duel broadcast
    for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
    {
        if (curTime - it->timestamp > 3.5)
            break;

        if (it->isServer == isServer && it->isCommand == isCommand)
        {
            if (it->text == cleanText)
            {
                bool senderMatch = (it->sender == cleanSender || cleanSender.empty() || it->sender.empty());

                if (senderMatch)
                {
                    if (it->sender.empty() && !cleanSender.empty())
                    {
                        it->sender = cleanSender;
                    }
                    if (it->prefix == "[CHAT]" && (cleanPrefix == "[ALL]" || cleanPrefix == "[TEAM]"))
                    {
                        it->prefix = cleanPrefix;
                        it->isTeam = isTeam;
                        it->r = r; it->g = g; it->b = b;
                    }
                    return; // Drop duplicate!
                }
            }
        }
    }

    LiveChatMessage msg;
    msg.clientIndex = clientIndex;
    msg.prefix = cleanPrefix.empty() ? (isCommand ? "[CMD]" : (isServer ? "[SERVER]" : (isTeam ? "[TEAM]" : "[ALL]"))) : cleanPrefix;
    msg.sender = cleanSender;
    msg.text = cleanText;
    msg.r = r;
    msg.g = g;
    msg.b = b;
    msg.isTeam = isTeam;
    msg.isServer = isServer;
    msg.isCommand = isCommand;
    msg.timestamp = curTime;

    m_messages.push_back(std::move(msg));
    while (m_messages.size() > 16)
    {
        m_messages.pop_front();
    }

    // Mirror to game console (~):
    if (gEngfuncs.pfnConsolePrint != nullptr)
    {
        std::string con = cleanSender.empty() ? (cleanText + "\n") : (cleanSender + " : " + cleanText + "\n");
        gEngfuncs.pfnConsolePrint(con.c_str());
    }
}

namespace
{
    bool IsSlashCommand(std::string_view str)
    {
        while (!str.empty() && (str.front() == ' ' || str.front() == '\t' || str.front() == '"' || str.front() == '\''))
            str.remove_prefix(1);
        return !str.empty() && str.front() == '/';
    }

    bool IsKnownServerTag(std::string_view tag)
    {
        std::string s(tag);
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        return (s == "[gameland]" || s.starts_with("[amx") ||
                s == "[server]" || s == "[info]" || s == "[notice]" ||
                s == "[vip]" || s == "[admin]" || s == "[rules]" ||
                s == "[top15]" || s == "[rank]" || s == "[host]" ||
                s == "[announcement]" || s == "[radio]" || s == "[broadcast]");
    }

    bool IsKnownServerPrefix(std::string_view str)
    {
        std::string s(str);
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        return (s.starts_with("[gameland]") || s.starts_with("[amx") ||
                s.starts_with("[server]") || s.starts_with("[info]") ||
                s.starts_with("[notice]") || s.starts_with("[vip]") ||
                s.starts_with("[admin]") || s.starts_with("[rules]") ||
                s.starts_with("[top15]") || s.starts_with("[rank]") ||
                s.starts_with("[host]") || s.starts_with("[announcement]") ||
                s.starts_with("server") || s.starts_with("console") ||
                s.starts_with("* ") || s.starts_with("/"));
    }

    bool IsConnectedPlayer(int clientIndex, const std::string& rawSenderName)
    {
        std::string senderName = rawSenderName;
        TrimString(senderName);
        if (senderName.empty())
            return false;

        if (IsKnownServerPrefix(senderName))
            return false;

        std::string lowerSender = senderName;
        for (char& c : lowerSender) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lowerSender.find("restarted the round") != std::string::npos ||
            lowerSender.find("round restart") != std::string::npos ||
            lowerSender.find("connected") != std::string::npos)
        {
            return false;
        }

        auto stripTag = [](std::string& s, std::string_view tag) {
            size_t pos = s.find(tag);
            if (pos != std::string::npos) s.erase(pos, tag.length());
        };
        stripTag(senderName, "*DEAD*");
        stripTag(senderName, "(DEAD)");
        stripTag(senderName, "[DEAD]");
        stripTag(senderName, "(Terrorist)");
        stripTag(senderName, "[Terrorist]");
        stripTag(senderName, "(Counter-Terrorist)");
        stripTag(senderName, "[Counter-Terrorist]");
        stripTag(senderName, "(Spectator)");
        stripTag(senderName, "[Spectator]");
        stripTag(senderName, "(ALL)");
        stripTag(senderName, "[ALL]");
        TrimString(senderName);

        if (senderName.empty())
            return false;

        // 1. Check against local player's configured name
        const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "");
        if (myName != nullptr && *myName != 0)
        {
            std::string myNameClean = StripColorCodes(myName);
            TrimString(myNameClean);
            if (!myNameClean.empty())
            {
                if (EqualsCI(myNameClean, senderName) ||
                    senderName.find(myNameClean) != std::string::npos ||
                    myNameClean.find(senderName) != std::string::npos)
                {
                    return true;
                }
            }
        }

        // 2. Check if clientIndex has a matching player name
        if (clientIndex >= 1 && clientIndex <= 32 && gEngfuncs.pfnGetPlayerInfo != nullptr)
        {
            hud_player_info_t pinfo{};
            gEngfuncs.pfnGetPlayerInfo(clientIndex, &pinfo);
            if (pinfo.name != nullptr && pinfo.name[0] != '\0')
            {
                std::string pName = StripColorCodes(pinfo.name);
                TrimString(pName);
                if (!pName.empty())
                {
                    if (EqualsCI(pName, senderName) ||
                        senderName.find(pName) != std::string::npos ||
                        pName.find(senderName) != std::string::npos)
                    {
                        return true;
                    }
                }
            }
        }

        if (gEngfuncs.pfnGetPlayerInfo == nullptr)
            return false;

        // 3. Check all connected players 1..32 for senderName
        for (int i = 1; i <= 32; ++i)
        {
            hud_player_info_t pinfo{};
            gEngfuncs.pfnGetPlayerInfo(i, &pinfo);
            if (pinfo.name != nullptr && pinfo.name[0] != '\0')
            {
                std::string pName = StripColorCodes(pinfo.name);
                TrimString(pName);
                if (!pName.empty())
                {
                    if (EqualsCI(pName, senderName) ||
                        senderName.find(pName) != std::string::npos ||
                        pName.find(senderName) != std::string::npos)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void ResolvePlayerTeamColor(int clientIndex, const std::string& sender, float& r, float& g, float& b)
    {
        int playerTeam = 0;

        if (clientIndex >= 1 && clientIndex <= 32 && g_NitroApi != nullptr)
        {
            auto* clientData = g_NitroApi->GetClientData();
            if (clientData != nullptr && clientData->g_PlayerExtraInfo != nullptr)
            {
                playerTeam = clientData->g_PlayerExtraInfo[clientIndex].teamnumber;
            }
        }

        if (playerTeam == 0 && gEngfuncs.pfnGetPlayerInfo != nullptr)
        {
            for (int i = 1; i <= 32; ++i)
            {
                hud_player_info_t pinfo{};
                gEngfuncs.pfnGetPlayerInfo(i, &pinfo);
                if (pinfo.name != nullptr && pinfo.name[0] != '\0')
                {
                    std::string pName = StripColorCodes(pinfo.name);
                    TrimString(pName);
                    if (EqualsCI(pName, sender) || sender.find(pName) != std::string::npos || pName.find(sender) != std::string::npos)
                    {
                        if (g_NitroApi != nullptr)
                        {
                            auto* clientData = g_NitroApi->GetClientData();
                            if (clientData != nullptr && clientData->g_PlayerExtraInfo != nullptr)
                            {
                                playerTeam = clientData->g_PlayerExtraInfo[i].teamnumber;
                            }
                        }
                        if (playerTeam == 0 && pinfo.model != nullptr)
                        {
                            std::string model = pinfo.model;
                            for (char& c : model) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                            if (model.find("terror") != std::string::npos || model.find("leet") != std::string::npos ||
                                model.find("arctic") != std::string::npos || model.find("guerilla") != std::string::npos)
                            {
                                playerTeam = 1;
                            }
                            else if (model.find("urban") != std::string::npos || model.find("gsg9") != std::string::npos ||
                                     model.find("gign") != std::string::npos || model.find("sas") != std::string::npos ||
                                     model.find("vip") != std::string::npos)
                            {
                                playerTeam = 2;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (playerTeam == 1) // Terrorist (Phoenix Crimson)
        {
            r = 1.0f; g = 0.28f; b = 0.32f;
        }
        else if (playerTeam == 2) // Counter-Terrorist (Sky Azure)
        {
            r = 0.18f; g = 0.76f; b = 1.0f;
        }
        else if (playerTeam == 3) // Spectator
        {
            r = 0.92f; g = 0.80f; b = 0.30f;
        }
        else
        {
            // Esports Gold default
            r = 0.98f; g = 0.82f; b = 0.25f;
        }
    }

    void FormatServerMessage(LiveChatMessage& outMsg, std::string text)
    {
        TrimString(text);
        if (text.empty())
            return;

        if (IsSlashCommand(text))
        {
            outMsg.isCommand = true;
            outMsg.isServer = false;
            outMsg.isTeam = false;
            outMsg.prefix = "[CMD]";
            outMsg.text = text;
            outMsg.r = 0.85f;
            outMsg.g = 0.88f;
            outMsg.b = 0.92f;
            return;
        }

        outMsg.isServer = true;
        outMsg.isCommand = false;
        outMsg.isTeam = false;
        outMsg.sender.clear();
        outMsg.r = 1.0f;
        outMsg.g = 0.78f;
        outMsg.b = 0.20f; // Radiant Amber / Solar Gold

        std::string prefix = "[SERVER]";

        // Detect known bracketed prefixes ONLY (e.g. [GAMELAND], [AMX], [SERVER], [INFO], [VIP], [ADMIN], [RULES], [NOTICE], [TOP15], [RANK])
        if (text.front() == '[')
        {
            size_t closeBracket = text.find(']');
            if (closeBracket != std::string::npos && closeBracket <= 24)
            {
                std::string candidateTag = text.substr(0, closeBracket + 1);
                if (IsKnownServerTag(candidateTag))
                {
                    prefix = candidateTag;
                    text = text.substr(closeBracket + 1);
                    TrimString(text);
                }
            }
        }
        else if (text.starts_with("* "))
        {
            prefix = "[NOTICE]";
            text = text.substr(2);
            TrimString(text);
        }

        outMsg.prefix = prefix;
        outMsg.text = text.empty() ? prefix : text;
    }

    bool ParseChatPacket(int clientIndex, const std::vector<std::string>& strings, LiveChatMessage& outMsg)
    {
        if (strings.empty())
            return false;

        std::string s1 = (strings.size() > 0) ? strings[0] : "";
        std::string s2 = (strings.size() > 1) ? strings[1] : "";
        std::string s3 = (strings.size() > 2) ? strings[2] : "";
        std::string s4 = (strings.size() > 3) ? strings[3] : "";

        std::string clean1 = StripColorCodes(s1);
        std::string clean2 = StripColorCodes(s2);
        std::string clean3 = StripColorCodes(s3);
        std::string clean4 = StripColorCodes(s4);

        TrimString(clean1);
        TrimString(clean2);
        TrimString(clean3);
        TrimString(clean4);

        if (clean1.empty() && clean2.empty() && clean3.empty() && clean4.empty())
            return false;

        // 1. Tokens starting with '#'
        if (clean1.starts_with("#"))
        {
            if (clean1.starts_with("#Cstrike_Chat_"))
            {
                // Standard CS 1.6 player chat token
                bool isTeam = (clean1.find("_T") != std::string::npos || clean1.find("_CT") != std::string::npos);
                bool isDead = (clean1.find("Dead") != std::string::npos || clean1.find("*DEAD*") != std::string::npos);
                bool isSpec = (clean1.find("Spec") != std::string::npos);

                std::string prefix;
                if (isSpec) prefix = "[SPEC]";
                else if (isTeam) prefix = "[TEAM]";
                else prefix = "[ALL]";
                if (isDead) prefix = "*DEAD* " + prefix;

                std::string sender = clean2;
                std::string text = clean3;

                if (clean1.find("_Loc") != std::string::npos && !clean4.empty())
                {
                    if (!clean3.empty()) sender += " (" + clean3 + ")";
                    text = clean4;
                }

                if (text.empty() && !clean2.empty())
                {
                    text = clean2;
                    sender.clear();
                }

                if (IsSlashCommand(text))
                {
                    FormatServerMessage(outMsg, text);
                    outMsg.sender = sender;
                    return true;
                }

                float r = 0.98f, g = 0.82f, b = 0.25f;
                if (isTeam)
                {
                    r = 0.18f; g = 0.95f; b = 0.45f; // Emerald Green
                }
                else if (isSpec)
                {
                    r = 0.92f; g = 0.80f; b = 0.30f;
                }
                else
                {
                    ResolvePlayerTeamColor(clientIndex, sender, r, g, b);
                }

                outMsg.clientIndex = clientIndex;
                outMsg.prefix = prefix;
                outMsg.sender = sender;
                outMsg.text = text;
                outMsg.r = r; outMsg.g = g; outMsg.b = b;
                outMsg.isTeam = isTeam;
                outMsg.isServer = false;
                outMsg.isCommand = false;
                return true;
            }
            else
            {
                // Game event tokens (e.g. #Game_connected, #Game_join_terrorist, #Fire_in_the_hole, #Target_Bombed, etc.)
                std::string eventText;
                if (clean1 == "#Game_connected")
                    eventText = clean2 + " connected";
                else if (clean1 == "#Game_disconnected")
                    eventText = clean2 + " disconnected";
                else if (clean1 == "#Game_join_terrorist")
                    eventText = clean2 + " joined the Terrorists";
                else if (clean1 == "#Game_join_ct")
                    eventText = clean2 + " joined the Counter-Terrorists";
                else if (clean1 == "#Target_Bombed")
                    eventText = "Target has been bombed!";
                else if (clean1 == "#Bomb_Defused")
                    eventText = "Bomb has been defused!";
                else if (clean1 == "#CTs_Win")
                    eventText = "Counter-Terrorists Win!";
                else if (clean1 == "#Terrorists_Win")
                    eventText = "Terrorists Win!";
                else if (clean1 == "#Round_Draw")
                    eventText = "Round Draw!";
                else if (clean1.find("Fire_in_the_hole") != std::string::npos)
                {
                    outMsg.isServer = true;
                    outMsg.isCommand = false;
                    outMsg.isTeam = false;
                    outMsg.prefix = "[RADIO]";
                    outMsg.sender = clean2;
                    outMsg.text = "Fire in the hole!";
                    outMsg.r = 1.0f; outMsg.g = 0.78f; outMsg.b = 0.20f;
                    return true;
                }
                else
                {
                    std::string cleanToken = clean1;
                    if (cleanToken.starts_with("#")) cleanToken.erase(0, 1);
                    for (char& c : cleanToken) if (c == '_') c = ' ';
                    eventText = cleanToken;
                    if (!clean2.empty()) eventText += " " + clean2;
                    if (!clean3.empty()) eventText += " " + clean3;
                }

                FormatServerMessage(outMsg, eventText);
                return true;
            }
        }

        // 2. Pure server notices, server commands, announcements
        if (clean1.starts_with("/"))
        {
            FormatServerMessage(outMsg, clean1);
            return true;
        }

        std::string lower1 = clean1;
        for (char& c : lower1) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (IsKnownServerPrefix(clean1))
        {
            std::string combined = clean1;
            if (!clean2.empty()) combined += " " + clean2;
            if (!clean3.empty()) combined += " " + clean3;
            if (!clean4.empty()) combined += " " + clean4;
            FormatServerMessage(outMsg, combined);
            return true;
        }

        // 3. Formatted tokens with %s or %s1 (e.g. "%s", "%s1 : %s2", "%s : %s", "%s1 restarted the round")
        if (clean1.find("%s") != std::string::npos)
        {
            // Case 3A: Single payload via AMXX ColorChat (%s format with entire message payload in clean2)
            if ((clean1 == "%s" || clean1 == "%s1") && clean3.empty() && !clean2.empty())
            {
                size_t cPos = clean2.find(" : ");
                size_t sepLen = 3;
                if (cPos == std::string::npos) { cPos = clean2.find("  :  "); sepLen = 5; }
                if (cPos == std::string::npos) { cPos = clean2.find(": "); sepLen = 2; }

                if (cPos != std::string::npos && cPos < 48)
                {
                    std::string possibleSender = clean2.substr(0, cPos);
                    std::string possibleText = clean2.substr(cPos + sepLen);
                    TrimString(possibleSender);
                    TrimString(possibleText);

                    if (!IsKnownServerPrefix(possibleSender))
                    {
                        if (IsSlashCommand(possibleText))
                        {
                            FormatServerMessage(outMsg, possibleText);
                            outMsg.sender = possibleSender;
                            return true;
                        }

                        float r = 0.98f, g = 0.82f, b = 0.25f;
                        ResolvePlayerTeamColor(clientIndex, possibleSender, r, g, b);
                        outMsg.clientIndex = clientIndex;
                        outMsg.prefix = "[ALL]";
                        outMsg.sender = possibleSender;
                        outMsg.text = possibleText;
                        outMsg.r = r; outMsg.g = g; outMsg.b = b;
                        outMsg.isTeam = false;
                        outMsg.isServer = false;
                        outMsg.isCommand = false;
                        return true;
                    }
                }

                // If not player chat with colon, it's a server message! (e.g. "[GAMELAND] [GL] PLAYER restarted the round.")
                FormatServerMessage(outMsg, clean2);
                return true;
            }

            std::string sender = clean2;
            std::string text = clean3;
            if (text.empty() && !clean4.empty()) text = clean4;
            if (text.empty() && !clean2.empty())
            {
                size_t cPos = clean2.find(" : ");
                size_t sepLen = 3;
                if (cPos == std::string::npos) { cPos = clean2.find("  :  "); sepLen = 5; }
                if (cPos == std::string::npos) { cPos = clean2.find(": "); sepLen = 2; }

                if (cPos != std::string::npos && cPos < 48)
                {
                    sender = clean2.substr(0, cPos);
                    text = clean2.substr(cPos + sepLen);
                    TrimString(sender);
                    TrimString(text);
                }
            }

            if (!text.empty() && IsSlashCommand(text))
            {
                FormatServerMessage(outMsg, text);
                outMsg.sender = sender;
                return true;
            }

            bool isServerSender = (IsKnownServerPrefix(sender) || (!IsConnectedPlayer(clientIndex, sender) && text.empty()));

            if (isServerSender)
            {
                // Reconstruct full server text
                std::string fullServerText = clean1;
                size_t p1 = fullServerText.find("%s1");
                if (p1 != std::string::npos)
                {
                    fullServerText.replace(p1, 3, clean2);
                    size_t p2 = fullServerText.find("%s2");
                    if (p2 != std::string::npos)
                        fullServerText.replace(p2, 3, clean3);
                    size_t p3 = fullServerText.find("%s3");
                    if (p3 != std::string::npos)
                        fullServerText.replace(p3, 3, clean4);
                }
                else
                {
                    size_t p = fullServerText.find("%s");
                    if (p != std::string::npos)
                    {
                        fullServerText.replace(p, 2, clean2);
                        size_t p_next = fullServerText.find("%s");
                        if (p_next != std::string::npos)
                            fullServerText.replace(p_next, 2, clean3);
                    }
                    else
                    {
                        if (!sender.empty()) fullServerText = sender + " " + text;
                        else fullServerText = text.empty() ? clean2 : text;
                    }
                }

                FormatServerMessage(outMsg, fullServerText);
                return true;
            }

            bool isTeam = (clean1.find("Terrorist") != std::string::npos || clean1.find("Counter") != std::string::npos || clean1.find("TEAM") != std::string::npos);
            bool isDead = (clean1.find("*DEAD*") != std::string::npos || clean1.find("Dead") != std::string::npos);
            bool isSpec = (clean1.find("Spec") != std::string::npos);

            std::string prefix;
            if (isSpec) prefix = "[SPEC]";
            else if (isTeam) prefix = "[TEAM]";
            else prefix = "[ALL]";
            if (isDead) prefix = "*DEAD* " + prefix;

            float r = 0.98f, g = 0.82f, b = 0.25f;
            if (isTeam)
            {
                r = 0.18f; g = 0.95f; b = 0.45f;
            }
            else if (isSpec)
            {
                r = 0.92f; g = 0.80f; b = 0.30f;
            }
            else
            {
                ResolvePlayerTeamColor(clientIndex, sender, r, g, b);
            }

            outMsg.clientIndex = clientIndex;
            outMsg.prefix = prefix;
            outMsg.sender = sender;
            outMsg.text = text.empty() ? clean2 : text;
            outMsg.r = r; outMsg.g = g; outMsg.b = b;
            outMsg.isTeam = isTeam;
            outMsg.isServer = false;
            outMsg.isCommand = false;
            return true;
        }

        // 4. Three-string player chat (clean1 = format/channel, clean2 = sender, clean3 = message)
        if (!clean2.empty() && !clean3.empty())
        {
            if (IsSlashCommand(clean3))
            {
                FormatServerMessage(outMsg, clean3);
                outMsg.sender = clean2;
                return true;
            }

            if (IsKnownServerPrefix(clean2))
            {
                std::string fullServerText = clean2 + " " + clean3;
                FormatServerMessage(outMsg, fullServerText);
                return true;
            }

            bool isTeam = (clean1.find("Terrorist") != std::string::npos || clean1.find("Counter") != std::string::npos ||
                           clean1.find("TEAM") != std::string::npos || clean2.find("(Terrorist)") != std::string::npos ||
                           clean2.find("(Counter-Terrorist)") != std::string::npos);
            bool isDead = (clean1.find("*DEAD*") != std::string::npos || clean2.find("*DEAD*") != std::string::npos);
            bool isSpec = (clean1.find("Spec") != std::string::npos || clean2.find("(Spectator)") != std::string::npos);

            std::string cleanPlayerSender = clean2;
            auto stripTag = [](std::string& s, std::string_view tag) {
                size_t pos = s.find(tag);
                if (pos != std::string::npos) s.erase(pos, tag.length());
            };
            stripTag(cleanPlayerSender, "*DEAD*");
            stripTag(cleanPlayerSender, "(DEAD)");
            stripTag(cleanPlayerSender, "[DEAD]");
            stripTag(cleanPlayerSender, "(Terrorist)");
            stripTag(cleanPlayerSender, "[Terrorist]");
            stripTag(cleanPlayerSender, "(Counter-Terrorist)");
            stripTag(cleanPlayerSender, "[Counter-Terrorist]");
            stripTag(cleanPlayerSender, "(Spectator)");
            stripTag(cleanPlayerSender, "[Spectator]");
            stripTag(cleanPlayerSender, "(ALL)");
            stripTag(cleanPlayerSender, "[ALL]");
            TrimString(cleanPlayerSender);

            std::string prefix;
            if (isSpec) prefix = "[SPEC]";
            else if (isTeam) prefix = "[TEAM]";
            else prefix = "[ALL]";
            if (isDead) prefix = "*DEAD* " + prefix;

            float r = 0.98f, g = 0.82f, b = 0.25f;
            if (isTeam)
            {
                r = 0.18f; g = 0.95f; b = 0.45f;
            }
            else if (isSpec)
            {
                r = 0.92f; g = 0.80f; b = 0.30f;
            }
            else
            {
                ResolvePlayerTeamColor(clientIndex, cleanPlayerSender, r, g, b);
            }

            outMsg.clientIndex = clientIndex;
            outMsg.prefix = prefix;
            outMsg.sender = cleanPlayerSender.empty() ? clean2 : cleanPlayerSender;
            outMsg.text = clean3;
            outMsg.r = r; outMsg.g = g; outMsg.b = b;
            outMsg.isTeam = isTeam;
            outMsg.isServer = false;
            outMsg.isCommand = false;
            return true;
        }

        // 5. Two-string chat (clean1 = sender, clean2 = message)
        if (!clean1.empty() && !clean2.empty() && clean3.empty())
        {
            if (IsSlashCommand(clean2))
            {
                FormatServerMessage(outMsg, clean2);
                outMsg.sender = clean1;
                return true;
            }

            if (IsKnownServerPrefix(clean1))
            {
                std::string fullServerText = clean1 + " " + clean2;
                FormatServerMessage(outMsg, fullServerText);
                return true;
            }

            bool isTeam = (clean1.find("(Terrorist)") != std::string::npos || clean1.find("(Counter-Terrorist)") != std::string::npos);
            bool isDead = (clean1.find("*DEAD*") != std::string::npos || clean1.find("Dead") != std::string::npos);
            bool isSpec = (clean1.find("(Spectator)") != std::string::npos || clean1.find("Spec") != std::string::npos);

            std::string cleanPlayerSender = clean1;
            auto stripTag = [](std::string& s, std::string_view tag) {
                size_t pos = s.find(tag);
                if (pos != std::string::npos) s.erase(pos, tag.length());
            };
            stripTag(cleanPlayerSender, "*DEAD*");
            stripTag(cleanPlayerSender, "(DEAD)");
            stripTag(cleanPlayerSender, "[DEAD]");
            stripTag(cleanPlayerSender, "(Terrorist)");
            stripTag(cleanPlayerSender, "[Terrorist]");
            stripTag(cleanPlayerSender, "(Counter-Terrorist)");
            stripTag(cleanPlayerSender, "[Counter-Terrorist]");
            stripTag(cleanPlayerSender, "(Spectator)");
            stripTag(cleanPlayerSender, "[Spectator]");
            stripTag(cleanPlayerSender, "(ALL)");
            stripTag(cleanPlayerSender, "[ALL]");
            TrimString(cleanPlayerSender);

            std::string prefix;
            if (isSpec) prefix = "[SPEC]";
            else if (isTeam) prefix = "[TEAM]";
            else prefix = "[ALL]";
            if (isDead) prefix = "*DEAD* " + prefix;

            float r = 0.98f, g = 0.82f, b = 0.25f;
            if (isTeam)
            {
                r = 0.18f; g = 0.95f; b = 0.45f;
            }
            else if (isSpec)
            {
                r = 0.92f; g = 0.80f; b = 0.30f;
            }
            else
            {
                ResolvePlayerTeamColor(clientIndex, cleanPlayerSender, r, g, b);
            }

            outMsg.clientIndex = clientIndex;
            outMsg.prefix = prefix;
            outMsg.sender = cleanPlayerSender.empty() ? clean1 : cleanPlayerSender;
            outMsg.text = clean2;
            outMsg.r = r; outMsg.g = g; outMsg.b = b;
            outMsg.isTeam = isTeam;
            outMsg.isServer = false;
            outMsg.isCommand = false;
            return true;
        }

        // 6. Single-string chat with " : " or ": "
        std::string fullLine = !clean1.empty() ? clean1 : clean2;
        size_t colonPos = fullLine.find(" : ");
        size_t sepLen = 3;
        if (colonPos == std::string::npos)
        {
            colonPos = fullLine.find("  :  ");
            sepLen = 5;
        }
        if (colonPos == std::string::npos)
        {
            colonPos = fullLine.find(": ");
            sepLen = 2;
        }

        if (colonPos != std::string::npos && colonPos > 0 && colonPos < 64)
        {
            std::string rawSender = fullLine.substr(0, colonPos);
            std::string rawMsg = fullLine.substr(colonPos + sepLen);
            TrimString(rawSender);
            TrimString(rawMsg);

            if (IsSlashCommand(rawMsg))
            {
                FormatServerMessage(outMsg, rawMsg);
                outMsg.sender = rawSender;
                return true;
            }

            if (IsKnownServerPrefix(rawSender))
            {
                FormatServerMessage(outMsg, fullLine);
                return true;
            }

            bool isTeam = (rawSender.find("(Terrorist)") != std::string::npos ||
                           rawSender.find("(Counter-Terrorist)") != std::string::npos);
            bool isDead = (rawSender.find("*DEAD*") != std::string::npos);
            bool isSpec = (rawSender.find("(Spectator)") != std::string::npos);

            std::string prefix;
            if (isSpec) prefix = "[SPEC]";
            else if (isTeam) prefix = "[TEAM]";
            else prefix = "[ALL]";
            if (isDead) prefix = "*DEAD* " + prefix;

            float r = 0.98f, g = 0.82f, b = 0.25f;
            if (isTeam)
            {
                r = 0.18f; g = 0.95f; b = 0.45f;
            }
            else if (isSpec)
            {
                r = 0.92f; g = 0.80f; b = 0.30f;
            }
            else
            {
                ResolvePlayerTeamColor(clientIndex, rawSender, r, g, b);
            }

            outMsg.clientIndex = clientIndex;
            outMsg.prefix = prefix;
            outMsg.sender = rawSender;
            outMsg.text = rawMsg;
            outMsg.r = r; outMsg.g = g; outMsg.b = b;
            outMsg.isTeam = isTeam;
            outMsg.isServer = false;
            outMsg.isCommand = false;
            return true;
        }

        // 7. Any other text -> treat as server broadcast / announcement!
        std::string anyText = clean1;
        if (!clean2.empty()) anyText += " " + clean2;
        if (!clean3.empty()) anyText += " " + clean3;
        if (!clean4.empty()) anyText += " " + clean4;
        TrimString(anyText);
        if (!anyText.empty())
        {
            FormatServerMessage(outMsg, anyText);
            return true;
        }

        return false;
    }
}

bool ModernChat::OnSayTextPacket(int clientIndex, const std::vector<std::string>& strings)
{
    LiveChatMessage msg;
    if (!ParseChatPacket(clientIndex, strings, msg))
        return false;

    if (msg.text.empty())
        return false;

    if (!msg.isServer)
    {
        // Safe deduplication against local player's recent send (for both chat and commands)
        cl_entity_t* localPlayer = (gEngfuncs.GetLocalPlayer ? gEngfuncs.GetLocalPlayer() : nullptr);
        int localIndex = (localPlayer != nullptr) ? localPlayer->index : -1;
        const double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);
        const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "");

        bool isLocalSender = false;
        if (localIndex > 0 && (clientIndex == localIndex || msg.clientIndex == localIndex))
            isLocalSender = true;
        if (myName != nullptr && *myName != 0)
        {
            if (EqualsCI(msg.sender, myName) ||
                msg.sender.find(myName) != std::string::npos ||
                std::string(myName).find(msg.sender) != std::string::npos)
            {
                isLocalSender = true;
            }
        }

        if (isLocalSender && !m_lastLocalSentText.empty() && (curTime - m_lastLocalSentTime) < 3.5)
        {
            if (msg.text == m_lastLocalSentText ||
                msg.text.find(m_lastLocalSentText) != std::string::npos ||
                m_lastLocalSentText.find(msg.text) != std::string::npos)
            {
                return true; // Already displayed locally, suppress network echo!
            }
        }
    }

    AddChatMessage(msg.clientIndex, msg.prefix, msg.sender, msg.text, msg.r, msg.g, msg.b, msg.isTeam, msg.isServer, msg.isCommand);
    return true;
}

bool ModernChat::OnTextMsgPacket(const std::vector<std::string>& strings)
{
    LiveChatMessage msg;
    if (!ParseChatPacket(0, strings, msg))
        return false;

    if (msg.text.empty())
        return false;

    if (!msg.isServer)
    {
        // Safe deduplication against local player's recent send (for both chat and commands)
        const double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);
        const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "");

        bool isLocalSender = false;
        if (myName != nullptr && *myName != 0)
        {
            if (EqualsCI(msg.sender, myName) ||
                msg.sender.find(myName) != std::string::npos ||
                std::string(myName).find(msg.sender) != std::string::npos)
            {
                isLocalSender = true;
            }
        }

        if (isLocalSender && !m_lastLocalSentText.empty() && (curTime - m_lastLocalSentTime) < 3.5)
        {
            if (msg.text == m_lastLocalSentText ||
                msg.text.find(m_lastLocalSentText) != std::string::npos ||
                m_lastLocalSentText.find(msg.text) != std::string::npos)
            {
                return true; // Already displayed locally, suppress network echo!
            }
        }
    }

    AddChatMessage(msg.clientIndex, msg.prefix, msg.sender, msg.text, msg.r, msg.g, msg.b, msg.isTeam, msg.isServer, msg.isCommand);
    return true;
}

void ModernChat::OnSayText(int clientIndex, const std::string& str1, const std::string& str2, const std::string& str3, const std::string& str4)
{
    std::vector<std::string> strings;
    strings.push_back(str1);
    if (!str2.empty() || !str3.empty() || !str4.empty()) strings.push_back(str2);
    if (!str3.empty() || !str4.empty()) strings.push_back(str3);
    if (!str4.empty()) strings.push_back(str4);

    OnSayTextPacket(clientIndex, strings);
}

void ModernChat::OnTextMsg(const std::string& formattedMsg)
{
    std::vector<std::string> strings = { formattedMsg };
    OnTextMsgPacket(strings);
}

void ModernChat::OnLocalPlayerSend(ModernChatMode mode, const std::string& message)
{
    const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "Me");
    const bool isTeam = (mode == ModernChatMode::SayTeam);
    std::string prefix = isTeam ? "[TEAM]" : "[ALL]";

    float r = 0.98f;
    float g = 0.82f;
    float b = 0.25f;

    cl_entity_t* localPlayer = (gEngfuncs.GetLocalPlayer ? gEngfuncs.GetLocalPlayer() : nullptr);
    int localIndex = (localPlayer != nullptr) ? localPlayer->index : 0;

    if (localPlayer != nullptr && gEngfuncs.pfnGetPlayerInfo != nullptr)
    {
        hud_player_info_t localInfo{};
        gEngfuncs.pfnGetPlayerInfo(localPlayer->index, &localInfo);
        if (localInfo.name != nullptr && localInfo.name[0] != '\0')
        {
            myName = localInfo.name;
        }
    }

    if (isTeam)
    {
        // When sending with 'u', team chat is strictly vibrant Emerald Green!
        r = 0.18f;
        g = 0.95f;
        b = 0.45f;
    }
    else
    {
        ResolvePlayerTeamColor(localIndex, (myName && *myName) ? myName : "Me", r, g, b);
    }

    AddChatMessage(localIndex, prefix, (myName && *myName) ? myName : "Me", message, r, g, b, isTeam, false, false);
}

int ModernChat::HandleKey(int down, int keynum, const char* /*pszCurrentBinding*/)
{
    if (!IsOpen())
        return 1;

    if (down == 0)
        return 0; // Ingest key releases while chat is open

    // 1. Mouse Controls (Send with Left-Click, Cancel with Right-Click)
    constexpr int kMouse1 = 241;
    constexpr int kMouse2 = 242;

    if (keynum == kMouse1) // Left Click -> Instant Send message!
    {
        Send();
        return 0;
    }

    if (keynum == kMouse2) // Right Click -> Instant Cancel!
    {
        Cancel();
        return 0;
    }

    if (keynum >= 243 && keynum <= 245)
    {
        return 0;
    }

    // 2. Keyboard Navigation
    if (keynum == 13 || keynum == 250) // Enter or NumPad Enter
    {
        Send();
        return 0;
    }

    if (keynum == 27) // Escape
    {
        Cancel();
        return 0;
    }

    if (keynum == 8 || keynum == 127) // Backspace
    {
        PopUtf8Char(m_buffer);
        return 0;
    }

    if (keynum == 9) // Tab: toggle between All / Team chat
    {
        ToggleMode();
        return 0;
    }

    if (keynum == 16 || keynum == 17) // Shift / Ctrl
    {
        return 0;
    }

#ifdef _WIN32
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (isCtrl)
    {
        if (keynum == 'v' || keynum == 'V')
        {
            PasteClipboardToChat(m_buffer, 120);
            return 0;
        }
        if (keynum == 'a' || keynum == 'A')
        {
            m_buffer.clear();
            return 0;
        }
    }
#endif

    // 3. Typing characters
    if (keynum >= 32 && keynum <= 126)
    {
        char ch = static_cast<char>(keynum);

#ifdef _WIN32
        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool isCaps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

        if (ch >= 'a' && ch <= 'z')
        {
            if (isShift ^ isCaps)
                ch = static_cast<char>(ch - 'a' + 'A');
        }
        else if (isShift)
        {
            switch (ch)
            {
                case '1': ch = '!'; break;
                case '2': ch = '@'; break;
                case '3': ch = '#'; break;
                case '4': ch = '$'; break;
                case '5': ch = '%'; break;
                case '6': ch = '^'; break;
                case '7': ch = '&'; break;
                case '8': ch = '*'; break;
                case '9': ch = '('; break;
                case '0': ch = ')'; break;
                case '-': ch = '_'; break;
                case '=': ch = '+'; break;
                case '[': ch = '{'; break;
                case ']': ch = '}'; break;
                case ';': ch = ':'; break;
                case '\'': ch = '"'; break;
                case ',': ch = '<'; break;
                case '.': ch = '>'; break;
                case '/': ch = '?'; break;
                case '`': ch = '~'; break;
                case '\\': ch = '|'; break;
                default: break;
            }
        }
#endif

        if (m_buffer.size() < 120)
        {
            m_buffer.push_back(ch);
        }
        return 0;
    }

    return 0; // Ingest all other keys while typing
}

void ModernChat::Draw(int scrW, int scrH)
{
    float scale = static_cast<float>(scrH) / 600.0f;
    if (scale < 1.0f) scale = 1.0f;

    const double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);

    m_barW = std::min(scrW - 48, static_cast<int>(540.0f * scale + 0.5f));
    m_barH = static_cast<int>(38.0f * scale + 0.5f);
    m_barX = 24;

    int targetBarY = scrH - static_cast<int>(180.0f * scale + 0.5f);

    // =========================================================================
    // PART A: DYNAMIC RECENT CHAT FEED (سیستم نمایش دو طبقه‌ای مدرن پیام‌ها)
    // طبقه پایین: پیام‌های سرور و دستورات با استایل مشکی مات (Stealth Black)
    // طبقه بالا: پیام‌های تیمی و همگانی بازیکنان با کادرهای سبز زمردی و آبسیدین
    // =========================================================================
    int currentMsgBottomY = targetBarY - 8;
    constexpr double kMessageLifetime = 8.0;

    // -------------------------------------------------------------------------
    // Tier 1: LOWER TIER - SERVER MESSAGES & COMMANDS (پایین‌تر از تکست‌های تیمی و آل)
    // -------------------------------------------------------------------------
    int drawnServerCount = 0;
    for (int i = static_cast<int>(m_messages.size()) - 1; i >= 0 && drawnServerCount < 5; --i)
    {
        const auto& m = m_messages[i];
        if (!m.isServer && !m.isCommand)
            continue;

        double age = curTime - m.timestamp;
        int alpha = 245;
        if (!IsOpen())
        {
            if (age > kMessageLifetime)
                continue;
            if (age > 6.0)
                alpha = static_cast<int>(245.0f * (1.0f - static_cast<float>(age - 6.0) / 2.0f));
        }
        if (alpha <= 6)
            continue;

        float enterT = std::min(1.0f, static_cast<float>(age / 0.20f));
        float slideEase = 1.0f - std::pow(1.0f - enterT, 3.0f);
        int slideOffsetX = static_cast<int>((1.0f - slideEase) * -38.0f);

        int lineH = static_cast<int>(24.0f * scale + 0.5f);
        currentMsgBottomY -= (lineH + 4);

        int prefixW = 0, senderW = 0, textW = 0, dummyH = 0;
        if (!m.prefix.empty()) GetTextSize(m.prefix.c_str(), prefixW, dummyH);
        if (!m.sender.empty()) GetTextSize(m.sender.c_str(), senderW, dummyH);
        GetTextSize(m.text.c_str(), textW, dummyH);

        int totalContentW = (m.prefix.empty() ? 0 : (prefixW + 14)) +
                            (m.sender.empty() ? 0 : (senderW + 18)) +
                            textW + 30;
        int cardW = std::min(scrW - 48, std::max(static_cast<int>(260.0f * scale), totalContentW));
        int cardX = m_barX + slideOffsetX;

        // 1. Soft Outer Ambient Drop-Shadow
        DrawBox(cardX - 1, currentMsgBottomY - 1, cardW + 2, lineH + 2, 0, 0, 0, (alpha * 160) / 255);

        if (m.isServer)
        {
            // =================================================================
            // DISTINCT SERVER THEME: Radiant Cyber Amber / Solar Gold Glass
            // =================================================================
            // 2. Deep Solar Slate Glass Body
            DrawBox(cardX, currentMsgBottomY, cardW, lineH, 26, 18, 8, (alpha * 235) / 255);

            // 3. Top Specular Gold Hairline
            DrawBox(cardX, currentMsgBottomY, cardW, 1, 255, 175, 35, (alpha * 215) / 255);

            // 4. Bottom Deep Bronze Accent
            DrawBox(cardX, currentMsgBottomY + lineH - 1, cardW, 1, 65, 42, 16, (alpha * 180) / 255);

            // 5. Right Warm Bronze Edge
            DrawBox(cardX + cardW - 1, currentMsgBottomY, 1, lineH, 75, 50, 20, (alpha * 130) / 255);

            // 6. Left Indicator Pillar (Luminous Solar Amber)
            DrawBox(cardX, currentMsgBottomY, 3, lineH, 255, 185, 45, alpha);

            // 7. Base Progress Line (Amber Gold)
            if (!IsOpen())
            {
                float remaining = std::clamp(1.0f - static_cast<float>(age / kMessageLifetime), 0.0f, 1.0f);
                int progW = static_cast<int>((cardW - 4) * remaining);
                if (progW > 0)
                {
                    DrawBox(cardX + 2, currentMsgBottomY + lineH - 2, progW, 1, 255, 195, 60, (alpha * 180) / 255);
                }
            }

            int posX = cardX + 10;
            int textY = currentMsgBottomY + (lineH - 13) / 2;

            // 8. Solar Bronze Badge Pill
            if (!m.prefix.empty())
            {
                int badgePad = 5;
                int badgeW = prefixW + (badgePad * 2);
                int badgeH = lineH - 6;
                int badgeY = currentMsgBottomY + 3;

                DrawBox(posX, badgeY, badgeW, badgeH, 75, 45, 10, (alpha * 230) / 255);
                DrawBox(posX, badgeY, badgeW, 1, 255, 175, 35, (alpha * 240) / 255);
                DrawBox(posX, badgeY + badgeH - 1, badgeW, 1, 255, 175, 35, (alpha * 150) / 255);

                DrawTextWithShadow(posX + badgePad, textY, m.prefix.c_str(), 1.0f, 0.84f, 0.35f); // Luminous Gold text
                posX += badgeW + 8;
            }

            if (!m.sender.empty())
            {
                DrawTextWithShadow(posX, textY, m.sender.c_str(), 1.0f, 0.88f, 0.65f);
                posX += senderW + 2;
                DrawTextWithShadow(posX, textY, ":", 0.90f, 0.75f, 0.40f);
                posX += 8;
            }

            // 9. Message Text in Warm Crystal Ivory-White
            DrawTextWithShadow(posX, textY, m.text.c_str(), 1.0f, 0.98f, 0.94f);
        }
        else // m.isCommand (Player Commands like /r, /rs, /top15)
        {
            // =================================================================
            // STEALTH COMMAND THEME: Matte Jet-Black Glass
            // =================================================================
            DrawBox(cardX, currentMsgBottomY, cardW, lineH, 5, 5, 8, (alpha * 245) / 255);
            DrawBox(cardX, currentMsgBottomY, cardW, 1, 75, 80, 90, (alpha * 160) / 255);
            DrawBox(cardX, currentMsgBottomY + lineH - 1, cardW, 1, 30, 32, 38, (alpha * 180) / 255);
            DrawBox(cardX + cardW - 1, currentMsgBottomY, 1, lineH, 45, 48, 55, (alpha * 130) / 255);
            DrawBox(cardX, currentMsgBottomY, 3, lineH, 130, 135, 145, alpha);

            if (!IsOpen())
            {
                float remaining = std::clamp(1.0f - static_cast<float>(age / kMessageLifetime), 0.0f, 1.0f);
                int progW = static_cast<int>((cardW - 4) * remaining);
                if (progW > 0)
                {
                    DrawBox(cardX + 2, currentMsgBottomY + lineH - 2, progW, 1, 95, 100, 110, (alpha * 180) / 255);
                }
            }

            int posX = cardX + 10;
            int textY = currentMsgBottomY + (lineH - 13) / 2;

            if (!m.prefix.empty())
            {
                int badgePad = 4;
                int badgeW = prefixW + (badgePad * 2);
                int badgeH = lineH - 6;
                int badgeY = currentMsgBottomY + 3;

                DrawBox(posX, badgeY, badgeW, badgeH, 20, 20, 26, (alpha * 230) / 255);
                DrawBox(posX, badgeY, badgeW, 1, 70, 75, 85, (alpha * 240) / 255);

                DrawTextWithShadow(posX + badgePad, textY, m.prefix.c_str(), 0.85f, 0.88f, 0.92f);
                posX += badgeW + 8;
            }

            if (!m.sender.empty())
            {
                DrawTextWithShadow(posX, textY, m.sender.c_str(), 0.85f, 0.88f, 0.92f);
                posX += senderW + 2;
                DrawTextWithShadow(posX, textY, ":", 0.70f, 0.73f, 0.78f);
                posX += 8;
            }

            DrawTextWithShadow(posX, textY, m.text.c_str(), 0.98f, 0.98f, 1.0f);
        }

        drawnServerCount++;
    }

    if (drawnServerCount > 0)
    {
        currentMsgBottomY -= 4; // Distinct visual tier separator gap
    }

    // -------------------------------------------------------------------------
    // Tier 2: UPPER TIER - PLAYER CHAT MESSAGES (چت‌های تیمی و همگانی بازیکنان)
    // -------------------------------------------------------------------------
    int drawnPlayerCount = 0;
    for (int i = static_cast<int>(m_messages.size()) - 1; i >= 0 && drawnPlayerCount < 6; --i)
    {
        const auto& m = m_messages[i];
        if (m.isServer || m.isCommand)
            continue;

        double age = curTime - m.timestamp;
        int alpha = 245;
        if (!IsOpen())
        {
            if (age > kMessageLifetime)
                continue;
            if (age > 6.0)
                alpha = static_cast<int>(245.0f * (1.0f - static_cast<float>(age - 6.0) / 2.0f));
        }
        if (alpha <= 6)
            continue;

        float enterT = std::min(1.0f, static_cast<float>(age / 0.20f));
        float slideEase = 1.0f - std::pow(1.0f - enterT, 3.0f);
        int slideOffsetX = static_cast<int>((1.0f - slideEase) * -38.0f);

        int lineH = static_cast<int>(24.0f * scale + 0.5f);
        currentMsgBottomY -= (lineH + 4);

        int prefixW = 0, senderW = 0, textW = 0, dummyH = 0;
        if (!m.prefix.empty()) GetTextSize(m.prefix.c_str(), prefixW, dummyH);
        if (!m.sender.empty()) GetTextSize(m.sender.c_str(), senderW, dummyH);
        GetTextSize(m.text.c_str(), textW, dummyH);

        int totalContentW = (m.prefix.empty() ? 0 : (prefixW + 14)) +
                            (m.sender.empty() ? 0 : (senderW + 18)) +
                            textW + 30;
        int cardW = std::min(scrW - 48, std::max(static_cast<int>(260.0f * scale), totalContentW));
        int cardX = m_barX + slideOffsetX;

        // 1. Soft Outer Ambient Drop-Shadow
        DrawBox(cardX - 1, currentMsgBottomY - 1, cardW + 2, lineH + 2, 0, 0, 0, (alpha * 140) / 255);

        if (m.isTeam)
        {
            // Emerald Glass Body for Team Chat
            DrawBox(cardX, currentMsgBottomY, cardW, lineH, 10, 28, 18, (alpha * 225) / 255);
            DrawBox(cardX, currentMsgBottomY, cardW, 1, 46, 213, 115, (alpha * 220) / 255);
            DrawBox(cardX, currentMsgBottomY + lineH - 1, cardW, 1, 46, 213, 115, (alpha * 120) / 255);
            DrawBox(cardX + cardW - 1, currentMsgBottomY, 1, lineH, 46, 213, 115, (alpha * 150) / 255);
            DrawBox(cardX, currentMsgBottomY, 3, lineH, 46, 213, 115, alpha);
        }
        else
        {
            // Frosted Obsidian Glass Body for All Chat
            DrawBox(cardX, currentMsgBottomY, cardW, lineH, 12, 16, 26, (alpha * 220) / 255);
            DrawBox(cardX, currentMsgBottomY, cardW, 1, 255, 255, 255, (alpha * 35) / 255);
            DrawBox(cardX, currentMsgBottomY, 3, lineH,
                    static_cast<int>(m.r * 255), static_cast<int>(m.g * 255), static_cast<int>(m.b * 255), alpha);
        }

        // Base progress line
        if (!IsOpen())
        {
            float remaining = std::clamp(1.0f - static_cast<float>(age / kMessageLifetime), 0.0f, 1.0f);
            int progW = static_cast<int>((cardW - 4) * remaining);
            if (progW > 0)
            {
                int laserR = m.isTeam ? 46 : static_cast<int>(m.r * 255);
                int laserG = m.isTeam ? 213 : static_cast<int>(m.g * 255);
                int laserB = m.isTeam ? 115 : static_cast<int>(m.b * 255);
                DrawBox(cardX + 2, currentMsgBottomY + lineH - 2, progW, 1,
                        laserR, laserG, laserB, (alpha * 180) / 255);
            }
        }

        int posX = cardX + 10;
        int textY = currentMsgBottomY + (lineH - 13) / 2;

        // Channel Badge Pill
        if (!m.prefix.empty())
        {
            int badgePad = 4;
            int badgeW = prefixW + (badgePad * 2);
            int badgeH = lineH - 6;
            int badgeY = currentMsgBottomY + 3;

            int bgR = m.isTeam ? 20 : static_cast<int>(m.r * 80);
            int bgG = m.isTeam ? 85 : static_cast<int>(m.g * 80);
            int bgB = m.isTeam ? 45 : static_cast<int>(m.b * 80);

            int borderR = m.isTeam ? 46 : static_cast<int>(m.r * 255);
            int borderG = m.isTeam ? 213 : static_cast<int>(m.g * 255);
            int borderB = m.isTeam ? 115 : static_cast<int>(m.b * 255);

            DrawBox(posX, badgeY, badgeW, badgeH, bgR, bgG, bgB, (alpha * 190) / 255);
            DrawBox(posX, badgeY, badgeW, 1, borderR, borderG, borderB, (alpha * 220) / 255);

            float prefixR = m.isTeam ? 0.40f : 0.90f;
            float prefixG = m.isTeam ? 1.0f : 0.95f;
            float prefixB = m.isTeam ? 0.60f : 1.0f;
            DrawTextWithShadow(posX + badgePad, textY, m.prefix.c_str(), prefixR, prefixG, prefixB);
            posX += badgeW + 8;
        }

        // Sender Name
        if (!m.sender.empty())
        {
            float senderR = m.isTeam ? 0.35f : m.r;
            float senderG = m.isTeam ? 1.0f : m.g;
            float senderB = m.isTeam ? 0.55f : m.b;
            DrawTextWithShadow(posX, textY, m.sender.c_str(), senderR, senderG, senderB);
            posX += senderW + 2;
            DrawTextWithShadow(posX, textY, ":", 0.85f, 0.88f, 0.92f);
            posX += 8;
        }

        // Message Content
        const char* pDrawMsg = m.text.c_str();
        std::string safeRenderText;
        if (!m.sender.empty())
        {
            if (StartsWithCI(m.text, m.sender))
            {
                size_t idx = m.sender.length();
                while (idx < m.text.length() && (m.text[idx] == ' ' || m.text[idx] == '\t'))
                    idx++;
                if (idx < m.text.length() && m.text[idx] == ':')
                {
                    idx++;
                    while (idx < m.text.length() && (m.text[idx] == ' ' || m.text[idx] == '\t'))
                        idx++;
                    safeRenderText = m.text.substr(idx);
                    pDrawMsg = safeRenderText.c_str();
                }
            }
        }
        if (pDrawMsg[0] == ':')
        {
            size_t idx = 1;
            while (pDrawMsg[idx] == ' ' || pDrawMsg[idx] == '\t')
                idx++;
            safeRenderText = pDrawMsg + idx;
            pDrawMsg = safeRenderText.c_str();
        }

        DrawTextWithShadow(posX, textY, pDrawMsg, 0.96f, 0.97f, 1.0f);
        drawnPlayerCount++;
    }

    // =========================================================================
    // PART B: MODERN CHAT INPUT BAR (وقتی Y یا U فشرده شده است)
    // =========================================================================
    if (!IsOpen())
        return;

    // Fluid ease-out entrance animation (slides up smoothly)
    float enterProgress = std::min(1.0f, static_cast<float>((curTime - m_openTime) / 0.14));
    float ease = 1.0f - std::pow(1.0f - enterProgress, 3.0f);
    int slideOffsetY = static_cast<int>((1.0f - ease) * 24.0f);
    m_barY = targetBarY + slideOffsetY;

    // Dynamic breathing neon pulse for borders and badges
    const float pulse = static_cast<float>(0.5 + 0.5 * std::sin(curTime * 4.2));
    const int pulseAlpha = 190 + static_cast<int>(65.0f * pulse);

    // 1. Soft Outer Drop-Shadow
    DrawBox(m_barX - 2, m_barY - 2, m_barW + 4, m_barH + 4, 0, 0, 0, 160);

    // 2. Obsidian Glass Body
    DrawBox(m_barX, m_barY, m_barW, m_barH, 14, 18, 26, 235);

    // 3. Mode-Specific Breathing Header Accent Line
    const bool isTeam = (m_mode == ModernChatMode::SayTeam);
    if (isTeam)
    {
        DrawBox(m_barX, m_barY, m_barW, 2, 46, 213, 115, pulseAlpha); // Emerald Green
    }
    else
    {
        DrawBox(m_barX, m_barY, m_barW, 2, 0, 220, 255, pulseAlpha); // Neon Cyan
    }

    // 4. Subtle Border Hairlines
    DrawBox(m_barX, m_barY + m_barH - 1, m_barW, 1, 255, 255, 255, 25);
    DrawBox(m_barX, m_barY, 1, m_barH, 255, 255, 255, 25);
    DrawBox(m_barX + m_barW - 1, m_barY, 1, m_barH, 255, 255, 255, 25);

    // 5. Mode Badge with breathing glow:
    int badgeW = static_cast<int>(82.0f * scale + 0.5f);
    int badgeH = m_barH - 10;
    int badgeX = m_barX + 6;
    int badgeY = m_barY + 5;

    if (isTeam)
    {
        DrawBox(badgeX, badgeY, badgeW, badgeH, 25, 110, 60, 200);
        DrawBox(badgeX, badgeY, badgeW, 1, 46, 213, 115, pulseAlpha);
        DrawTextWithShadow(badgeX + 11, badgeY + (badgeH - 13) / 2, "[ TEAM ]", 0.35f, 1.0f, 0.55f);
    }
    else
    {
        DrawBox(badgeX, badgeY, badgeW, badgeH, 0, 105, 145, 200);
        DrawBox(badgeX, badgeY, badgeW, 1, 0, 220, 255, pulseAlpha);
        DrawTextWithShadow(badgeX + 15, badgeY + (badgeH - 13) / 2, "[ ALL ]", 0.3f, 0.95f, 1.0f);
    }

    // 6. Player Identity Tag:
    const char* pPlayerName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "");
    int nameW = 0, dummyH = 0;
    if (pPlayerName != nullptr && *pPlayerName != 0)
    {
        GetTextSize(pPlayerName, nameW, dummyH);
    }

    int inputX = badgeX + badgeW + 8;
    int inputY = m_barY + (m_barH - 13) / 2;

    if (nameW > 0)
    {
        DrawTextWithShadow(inputX, inputY, pPlayerName, 1.0f, 0.82f, 0.25f); // Esports gold name
        inputX += nameW + 2;
        DrawTextWithShadow(inputX, inputY, ":", 0.85f, 0.85f, 0.85f);
        inputX += 8;
    }

    // 7. Input Text Field with smooth horizontal auto-scroll across full width
    int inputMaxW = (m_barX + m_barW) - inputX - 16;
    const char* pDrawText = m_buffer.c_str();
    std::string visibleBuffer;
    int textW = 0, textH = 0;

    if (m_buffer.empty())
    {
        DrawTextWithShadow(inputX, inputY, "Say something...", 0.52f, 0.56f, 0.64f);
    }
    else
    {
        GetTextSize(pDrawText, textW, textH);
        if (textW > inputMaxW && inputMaxW > 20)
        {
            size_t startIdx = 0;
            while (startIdx < m_buffer.size())
            {
                visibleBuffer = m_buffer.substr(startIdx);
                GetTextSize(visibleBuffer.c_str(), textW, textH);
                if (textW <= inputMaxW)
                    break;
                startIdx++;
            }
            pDrawText = visibleBuffer.c_str();
        }
        DrawTextWithShadow(inputX, inputY, pDrawText, 0.98f, 0.98f, 1.0f);
    }

    // 8. Pulsing Glowing Cursor '|'
    bool blink = (static_cast<int>(curTime * 3.2) % 2) == 0;
    if (blink)
    {
        int curX = inputX + textW + 2;
        int curY = m_barY + 8;
        if (isTeam)
            DrawBox(curX, curY, 2, m_barH - 16, 46, 213, 115, 240);
        else
            DrawBox(curX, curY, 2, m_barH - 16, 0, 220, 255, 240);
    }

    // 9. Dynamic Character Progress Micro-Bar under Input Field
    float charRatio = std::clamp(static_cast<float>(m_buffer.size()) / 120.0f, 0.0f, 1.0f);
    int charBarW = static_cast<int>((m_barW - 12) * charRatio);
    int charBarR = (m_buffer.size() > 100) ? 255 : (m_buffer.size() > 70 ? 255 : (isTeam ? 46 : 0));
    int charBarG = (m_buffer.size() > 100) ? 65  : (m_buffer.size() > 70 ? 190 : (isTeam ? 213 : 220));
    int charBarB = (m_buffer.size() > 100) ? 65  : (m_buffer.size() > 70 ? 40  : (isTeam ? 115 : 255));
    if (charBarW > 0)
    {
        DrawBox(m_barX + 6, m_barY + m_barH - 2, charBarW, 1, charBarR, charBarG, charBarB, pulseAlpha);
    }

    // 10. Modern Dynamic Helper Ribbon Below
    int helpY = m_barY + m_barH + 4;
    int helpH = 18;
    DrawBox(m_barX, helpY, m_barW, helpH, 10, 14, 20, 200);
    DrawBox(m_barX, helpY, m_barW, 1, 255, 255, 255, 20);

    // Live character counter e.g. [ 12 / 120 ]
    char countBuf[32]{};
    std::snprintf(countBuf, sizeof(countBuf), "[ %zu / 120 ]", m_buffer.size());
    int countW = 0;
    GetTextSize(countBuf, countW, dummyH);

    DrawTextWithShadow(m_barX + 10, helpY + 3,
        "[Enter / Left-Click] Send   *   [Esc / Right-Click] Cancel   *   [Tab] Switch Channel",
        0.72f, 0.76f, 0.84f);

    float countR = (m_buffer.size() > 100) ? 1.0f : 0.50f;
    float countG = (m_buffer.size() > 100) ? 0.35f : 0.75f;
    float countB = (m_buffer.size() > 100) ? 0.35f : 0.90f;
    DrawTextWithShadow(m_barX + m_barW - countW - 10, helpY + 3, countBuf, countR, countG, countB);
}
