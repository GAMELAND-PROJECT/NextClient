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

            // Add to live modern chat feed immediately for instantaneous feedback
            OnLocalPlayerSend(m_mode, escaped);
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

void ModernChat::AddChatMessage(int clientIndex, const std::string& prefix, const std::string& sender, const std::string& text, float r, float g, float b, bool isTeam)
{
    std::string cleanText = StripColorCodes(text);
    std::string cleanSender = StripColorCodes(sender);
    std::string cleanPrefix = StripColorCodes(prefix);

    std::string realPlayerName;
    if (clientIndex >= 1 && clientIndex <= 32 && gEngfuncs.pfnGetPlayerInfo != nullptr)
    {
        hud_player_info_t pinfo{};
        gEngfuncs.pfnGetPlayerInfo(clientIndex, &pinfo);
        if (pinfo.name != nullptr && pinfo.name[0] != '\0')
        {
            realPlayerName = StripColorCodes(pinfo.name);
        }
    }

    CleanChatMessage(cleanSender, cleanText, realPlayerName);

    if (cleanText.empty() && cleanSender.empty())
        return;

    const double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);

    // Robust deduplication: prevent double messages from SayText + TextMsg duel broadcast
    for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
    {
        if (curTime - it->timestamp > 3.5)
            break;

        // Check if text matches exactly or as substring
        if (it->text == cleanText ||
            (!cleanText.empty() && !it->text.empty() &&
             (it->text.find(cleanText) != std::string::npos || cleanText.find(it->text) != std::string::npos)))
        {
            // If sender matches, or one of them is empty, or one contains the other:
            bool senderMatch = (it->sender == cleanSender || cleanSender.empty() || it->sender.empty() ||
                                it->sender.find(cleanSender) != std::string::npos || cleanSender.find(it->sender) != std::string::npos);

            if (senderMatch)
            {
                // If the new one has a better sender (not empty) and old was empty:
                if (it->sender.empty() && !cleanSender.empty())
                {
                    it->sender = cleanSender;
                }
                // If existing has generic [CHAT] but new one has specific [ALL] or [TEAM], upgrade prefix:
                if (it->prefix == "[CHAT]" && (cleanPrefix == "[ALL]" || cleanPrefix == "[TEAM]"))
                {
                    it->prefix = cleanPrefix;
                    it->isTeam = isTeam;
                    it->r = r; it->g = g; it->b = b;
                }
                // Drop duplicate!
                return;
            }
        }
    }

    LiveChatMessage msg;
    msg.clientIndex = clientIndex;
    msg.prefix = cleanPrefix;
    msg.sender = cleanSender;
    msg.text = cleanText;
    msg.r = r;
    msg.g = g;
    msg.b = b;
    msg.isTeam = isTeam;
    msg.timestamp = curTime;

    m_messages.push_back(std::move(msg));
    while (m_messages.size() > 10)
    {
        m_messages.pop_front();
    }
}

void ModernChat::OnSayText(int clientIndex, const std::string& str1, const std::string& str2, const std::string& str3, const std::string& str4)
{
    std::string clean1 = StripColorCodes(str1);
    std::string clean2 = StripColorCodes(str2);
    std::string clean3 = StripColorCodes(str3);
    std::string clean4 = StripColorCodes(str4);

    bool isTeam = false;
    float r = 0.98f, g = 0.82f, b = 0.25f; // Esports Gold default
    std::string prefix = "[ALL]";

    // Fetch canonical in-game player name from clientIndex (1..32)
    std::string realPlayerName;
    int playerTeam = 0;
    if (clientIndex >= 1 && clientIndex <= 32)
    {
        if (g_NitroApi != nullptr)
        {
            auto* clientData = g_NitroApi->GetClientData();
            if (clientData != nullptr && clientData->g_PlayerExtraInfo != nullptr)
            {
                playerTeam = clientData->g_PlayerExtraInfo[clientIndex].teamnumber;
            }
        }

        if (gEngfuncs.pfnGetPlayerInfo != nullptr)
        {
            hud_player_info_t pinfo{};
            gEngfuncs.pfnGetPlayerInfo(clientIndex, &pinfo);
            if (pinfo.name != nullptr && pinfo.name[0] != '\0')
            {
                realPlayerName = StripColorCodes(pinfo.name);
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
        }
    }

    std::string sender;
    std::string msg;

    // 1. Team Chat detection
    if (clean1.find("_T") != std::string::npos || clean1.find("Terrorist") != std::string::npos ||
        clean1.find("_CT") != std::string::npos || clean1.find("Counter") != std::string::npos)
    {
        isTeam = true;
        prefix = "[TEAM]";
        r = 0.18f; g = 0.95f; b = 0.45f; // Vibrant Emerald Green for friendly team chat

        // Always resolve sender cleanly: prefer canonical player name so teammate is NEVER anonymous
        if (!realPlayerName.empty())
        {
            sender = realPlayerName;
            if (!clean3.empty())
                msg = clean3;
            else if (!clean2.empty() && clean2 != realPlayerName)
                msg = clean2;
            else
                msg = clean3;
        }
        else
        {
            sender = !clean2.empty() ? clean2 : "Teammate";
            msg = clean3;
        }

        // CS 1.6 location format: if str4 (or str3) contains location (e.g. #Cstrike_Chat_T_Loc)
        std::string loc = clean4;
        if (loc.empty() && clean1.find("_Loc") != std::string::npos && !clean3.empty() && clean3 != msg)
        {
            loc = clean3;
        }
        if (!loc.empty())
        {
            sender += " (" + loc + ")";
        }
    }
    else if (clean1.find("Spec") != std::string::npos)
    {
        r = 0.92f; g = 0.80f; b = 0.30f; // Spectator cyber gold
        prefix = "[SPEC]";
        sender = !realPlayerName.empty() ? realPlayerName : (!clean2.empty() ? clean2 : "Spectator");
        msg = !clean3.empty() ? clean3 : clean2;
    }
    else
    {
        // 2. All Chat - resolve sender's team color (Terrorist / CT / Spec)
        prefix = "[ALL]";
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
            prefix = "[SPEC]";
        }

        // Determine sender name
        if (!clean2.empty() && clean2 != clean3 && clean2 != clean4)
        {
            sender = clean2;
        }
        else if (!realPlayerName.empty())
        {
            sender = realPlayerName;
        }
        else if (!clean2.empty())
        {
            sender = clean2;
        }

        // Determine message body across all packet formats
        if (!clean4.empty())
        {
            msg = clean4;
            if (!clean3.empty() && clean3 != sender && clean3 != realPlayerName)
            {
                sender += " (" + clean3 + ")";
            }
        }
        else if (!clean3.empty())
        {
            msg = clean3;
        }
        else if (!clean2.empty())
        {
            // Could be "sender : message" in clean2
            size_t colonPos = clean2.find(" : ");
            if (colonPos == std::string::npos) colonPos = clean2.find("  :  ");
            if (colonPos == std::string::npos) colonPos = clean2.find(":");
            if (colonPos != std::string::npos)
            {
                sender = clean2.substr(0, colonPos);
                size_t startMsg = colonPos + 1;
                while (startMsg < clean2.length() && (clean2[startMsg] == ' ' || clean2[startMsg] == '\t' || clean2[startMsg] == ':'))
                    startMsg++;
                msg = clean2.substr(startMsg);
            }
            else if (clean2 != realPlayerName && !realPlayerName.empty())
            {
                msg = clean2;
            }
        }
        else if (!clean1.empty() && !clean1.starts_with("#"))
        {
            // Single-string format in clean1 (AMX Mod X ColorChat)
            size_t colonPos = clean1.find(" : ");
            if (colonPos == std::string::npos) colonPos = clean1.find("  :  ");
            if (colonPos == std::string::npos) colonPos = clean1.find(":");
            if (colonPos != std::string::npos)
            {
                sender = clean1.substr(0, colonPos);
                size_t startMsg = colonPos + 1;
                while (startMsg < clean1.length() && (clean1[startMsg] == ' ' || clean1[startMsg] == '\t' || clean1[startMsg] == ':'))
                    startMsg++;
                msg = clean1.substr(startMsg);
            }
            else
            {
                prefix = "[SERVER]";
                sender.clear();
                msg = clean1;
                r = 0.15f; g = 0.95f; b = 0.55f;
            }
        }
    }

    if (clean1.find("Dead") != std::string::npos || clean1.find("*DEAD*") != std::string::npos)
    {
        prefix = "*DEAD* " + prefix;
    }

    // 3. Special CS format tokens
    if (clean1 == "#Cstrike_Name_Change")
    {
        prefix = "[INFO]";
        sender.clear();
        msg = clean2 + " is now known as " + clean3;
        r = 0.85f; g = 0.88f; b = 0.92f;
    }

    // 4. Safe echo deduplication: if server echoed our own recent message within 3.0s, skip duplicate
    cl_entity_t* localPlayer = (gEngfuncs.GetLocalPlayer ? gEngfuncs.GetLocalPlayer() : nullptr);
    int localIndex = (localPlayer != nullptr) ? localPlayer->index : -1;
    if (clientIndex == localIndex && localIndex > 0)
    {
        const double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);
        if (!m_lastLocalSentText.empty() && (curTime - m_lastLocalSentTime) < 3.0 && msg == m_lastLocalSentText)
        {
            return;
        }
    }

    CleanChatMessage(sender, msg, realPlayerName);
    AddChatMessage(clientIndex, prefix, sender, msg, r, g, b, isTeam);
}

void ModernChat::OnTextMsg(const std::string& formattedMsg)
{
    std::string cleaned = StripColorCodes(formattedMsg);
    if (cleaned.empty())
        return;

    bool isTeam = false;
    float r = 0.98f, g = 0.82f, b = 0.25f;
    std::string prefix = "[ALL]";
    std::string sender;
    std::string msg;

    if (cleaned.find("(Terrorist)") != std::string::npos || cleaned.find("(Counter-Terrorist)") != std::string::npos)
    {
        isTeam = true;
        prefix = "[TEAM]";
        r = 0.18f; g = 0.95f; b = 0.45f;
    }

    bool isDead = (cleaned.find("*DEAD*") != std::string::npos);

    // Look for separator " : " or "  :  " or ": "
    size_t colonPos = cleaned.find(" : ");
    size_t sepLen = 3;
    if (colonPos == std::string::npos)
    {
        colonPos = cleaned.find("  :  ");
        sepLen = 5;
    }
    if (colonPos == std::string::npos)
    {
        colonPos = cleaned.find(": ");
        sepLen = 2;
    }

    if (colonPos != std::string::npos)
    {
        std::string rawSender = cleaned.substr(0, colonPos);
        msg = cleaned.substr(colonPos + sepLen);

        // Strip team prefix tags from rawSender so only the clean player name remains
        size_t tagPos = rawSender.find("(Terrorist)");
        if (tagPos != std::string::npos) rawSender.erase(tagPos, 11);
        tagPos = rawSender.find("(Counter-Terrorist)");
        if (tagPos != std::string::npos) rawSender.erase(tagPos, 19);
        tagPos = rawSender.find("(Spectator)");
        if (tagPos != std::string::npos) rawSender.erase(tagPos, 11);
        tagPos = rawSender.find("*DEAD*");
        if (tagPos != std::string::npos) rawSender.erase(tagPos, 6);

        // Trim whitespace
        while (!rawSender.empty() && (rawSender.front() == ' ' || rawSender.front() == '\t'))
            rawSender.erase(0, 1);
        while (!rawSender.empty() && (rawSender.back() == ' ' || rawSender.back() == '\t'))
            rawSender.pop_back();

        sender = rawSender;
    }
    else
    {
        prefix = "[SERVER]";
        sender.clear();
        msg = cleaned;
        r = 0.15f; g = 0.95f; b = 0.55f;
    }

    if (isDead)
    {
        prefix = "*DEAD* " + prefix;
    }

    CleanChatMessage(sender, msg);

    // AddChatMessage will automatically deduplicate if SayText or local send already added this text!
    AddChatMessage(0, prefix, sender, msg, r, g, b, isTeam);

    if (gEngfuncs.pfnConsolePrint != nullptr)
    {
        std::string con = cleaned + "\n";
        gEngfuncs.pfnConsolePrint(con.c_str());
    }
}

void ModernChat::OnLocalPlayerSend(ModernChatMode mode, const std::string& message)
{
    const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "Me");
    const bool isTeam = (mode == ModernChatMode::SayTeam);
    std::string prefix = isTeam ? "[TEAM]" : "[ALL]";

    float r = 0.0f;
    float g = 0.85f;
    float b = 1.0f;

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
        if (localPlayer != nullptr && g_NitroApi != nullptr)
        {
            auto* clientData = g_NitroApi->GetClientData();
            if (clientData != nullptr && clientData->g_PlayerExtraInfo != nullptr && localIndex >= 1 && localIndex <= 32)
            {
                int team = clientData->g_PlayerExtraInfo[localIndex].teamnumber;
                if (team == 1) // Terrorist
                {
                    r = 1.0f; g = 0.28f; b = 0.32f;
                }
                else if (team == 2) // CT
                {
                    r = 0.18f; g = 0.76f; b = 1.0f;
                }
            }
        }
    }

    AddChatMessage(localIndex, prefix, (myName && *myName) ? myName : "Me", message, r, g, b, isTeam);
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
    // PART A: DYNAMIC RECENT CHAT FEED (سیستم نمایش زنده و پویای پیام‌ها)
    // =========================================================================
    int currentMsgBottomY = targetBarY - 8;
    int drawnMessages = 0;
    constexpr double kMessageLifetime = 8.0;

    for (int i = static_cast<int>(m_messages.size()) - 1; i >= 0 && drawnMessages < 6; --i)
    {
        const auto& m = m_messages[i];
        double age = curTime - m.timestamp;

        int alpha = 245;
        if (!IsOpen())
        {
            if (age > kMessageLifetime)
                continue;
            if (age > 6.0)
            {
                alpha = static_cast<int>(245.0f * (1.0f - static_cast<float>(age - 6.0) / 2.0f));
            }
        }

        if (alpha <= 6)
            continue;

        // Smooth kinetic ease-out slide-in from the left on arrival
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
            // 2. Emerald Glass Body for Team Chat (کادر و بدنه سبز رنگ چت تیم خودی)
            DrawBox(cardX, currentMsgBottomY, cardW, lineH, 10, 28, 18, (alpha * 225) / 255);

            // 3. Glowing Emerald Frame Borders (کادر سبز رنگ)
            DrawBox(cardX, currentMsgBottomY, cardW, 1, 46, 213, 115, (alpha * 220) / 255); // Top green accent
            DrawBox(cardX, currentMsgBottomY + lineH - 1, cardW, 1, 46, 213, 115, (alpha * 120) / 255); // Bottom
            DrawBox(cardX + cardW - 1, currentMsgBottomY, 1, lineH, 46, 213, 115, (alpha * 150) / 255); // Right
            DrawBox(cardX, currentMsgBottomY, 3, lineH, 46, 213, 115, alpha); // Left Neon Pillar
        }
        else
        {
            // 2. Frosted Obsidian Glass Body for All Chat
            DrawBox(cardX, currentMsgBottomY, cardW, lineH, 12, 16, 26, (alpha * 220) / 255);

            // 3. Top Hairline Specular Highlight
            DrawBox(cardX, currentMsgBottomY, cardW, 1, 255, 255, 255, (alpha * 35) / 255);

            // 4. Dynamic Left Team-Color Neon Pillar
            DrawBox(cardX, currentMsgBottomY, 3, lineH,
                    static_cast<int>(m.r * 255), static_cast<int>(m.g * 255), static_cast<int>(m.b * 255), alpha);
        }

        // 5. Dynamic Kinetic Laser Progress Line at base of card
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

        // 6. Channel / Status Badge Pill
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

        // 7. Sender Name with Drop-Shadow
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

        // 8. Message Content in Crystal Diamond White with Drop-Shadow
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

        drawnMessages++;
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
