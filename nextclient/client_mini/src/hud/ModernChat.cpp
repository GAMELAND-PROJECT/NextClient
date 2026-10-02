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
            // Strip CS 1.6 byte control characters (\x01 - \x04)
            if (static_cast<unsigned char>(c) >= 1 && static_cast<unsigned char>(c) <= 4)
                continue;
            if (c == '\r' || c == '\n')
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

    // Audio feedback: crisp subtle cyber blip
    if (gEngfuncs.pfnPlaySoundByName != nullptr)
        gEngfuncs.pfnPlaySoundByName("buttons/blip1.wav", 0.8f);

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

            if (gEngfuncs.pfnClientCmd != nullptr)
                gEngfuncs.pfnClientCmd(cmd);

            // Add to live modern chat feed immediately for instantaneous feedback
            OnLocalPlayerSend(m_mode, escaped);

            // Sound feedback for successful send
            if (gEngfuncs.pfnPlaySoundByName != nullptr)
                gEngfuncs.pfnPlaySoundByName("buttons/blip2.wav", 0.9f);
        }
    }

    Close();
}

void ModernChat::Cancel()
{
    // Sound feedback for cancel
    if (gEngfuncs.pfnPlaySoundByName != nullptr)
        gEngfuncs.pfnPlaySoundByName("buttons/button2.wav", 0.5f);

    Close();
}

void ModernChat::ToggleMode()
{
    if (m_mode == ModernChatMode::SayAll)
        m_mode = ModernChatMode::SayTeam;
    else if (m_mode == ModernChatMode::SayTeam)
        m_mode = ModernChatMode::SayAll;

    if (gEngfuncs.pfnPlaySoundByName != nullptr)
        gEngfuncs.pfnPlaySoundByName("buttons/lightswitch2.wav", 0.7f);
}

void ModernChat::AddChatMessage(int clientIndex, const std::string& prefix, const std::string& sender, const std::string& text, float r, float g, float b)
{
    if (text.empty() && sender.empty())
        return;

    LiveChatMessage msg;
    msg.clientIndex = clientIndex;
    msg.prefix = StripColorCodes(prefix);
    msg.sender = StripColorCodes(sender);
    msg.text = StripColorCodes(text);
    msg.r = r;
    msg.g = g;
    msg.b = b;
    msg.timestamp = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);

    m_messages.push_back(std::move(msg));
    while (m_messages.size() > 10)
    {
        m_messages.pop_front();
    }
}

void ModernChat::OnSayText(int clientIndex, const std::string& str1, const std::string& str2, const std::string& str3, const std::string& /*str4*/)
{
    float r = 0.98f, g = 0.82f, b = 0.25f; // Esports Gold default
    std::string prefix = "[ALL]";
    std::string sender = str2;
    std::string msg = str3;

    // Check player team from engine extra info if valid
    if (clientIndex >= 1 && clientIndex <= 32 && g_NitroApi != nullptr)
    {
        auto* clientData = g_NitroApi->GetClientData();
        if (clientData != nullptr)
        {
            int team = clientData->g_PlayerExtraInfo[clientIndex].teamnumber;
            if (team == 1) // TEAM_TERRORIST
            {
                r = 1.0f; g = 0.28f; b = 0.32f; // Vibrant Phoenix Crimson
            }
            else if (team == 2) // TEAM_CT
            {
                r = 0.18f; g = 0.76f; b = 1.0f; // Vibrant Sky Azure
            }
        }
    }

    if (str1.find("_T") != std::string::npos || str1.find("Terrorist") != std::string::npos)
    {
        r = 1.0f; g = 0.28f; b = 0.32f; // Terrorist vibrant red
        prefix = "[TEAM]";
    }
    else if (str1.find("_CT") != std::string::npos || str1.find("Counter") != std::string::npos)
    {
        r = 0.18f; g = 0.76f; b = 1.0f; // CT vibrant sky blue
        prefix = "[TEAM]";
    }
    else if (str1.find("Spec") != std::string::npos)
    {
        r = 0.92f; g = 0.80f; b = 0.30f; // Spectator cyber gold
        prefix = "[SPEC]";
    }

    if (str1.find("Dead") != std::string::npos || str1.find("*DEAD*") != std::string::npos)
    {
        prefix = "*DEAD* " + prefix;
    }

    // If str1 is not a format token (#Cstrike_Chat_...), handle custom AMX / Server message
    if (!str1.starts_with("#"))
    {
        if (str2.empty())
        {
            size_t colonPos = str1.find(" : ");
            if (colonPos != std::string::npos)
            {
                prefix = "[CHAT]";
                sender = str1.substr(0, colonPos);
                msg = str1.substr(colonPos + 3);
            }
            else
            {
                prefix = "[SERVER]";
                sender.clear();
                msg = str1;
                r = 0.15f; g = 0.95f; b = 0.55f; // Neon Emerald
            }
        }
        else
        {
            prefix = "[CHAT]";
            sender = str2;
            msg = str3;
        }
    }

    AddChatMessage(clientIndex, prefix, sender, msg, r, g, b);
}

void ModernChat::OnLocalPlayerSend(ModernChatMode mode, const std::string& message)
{
    const char* myName = (gEngfuncs.pfnGetCvarString != nullptr ? gEngfuncs.pfnGetCvarString("name") : "Me");
    std::string prefix = (mode == ModernChatMode::SayTeam) ? "[TEAM]" : "[ALL]";
    float r = (mode == ModernChatMode::SayTeam) ? 0.25f : 0.0f;
    float g = (mode == ModernChatMode::SayTeam) ? 0.95f : 0.85f;
    float b = (mode == ModernChatMode::SayTeam) ? 0.55f : 1.0f;

    cl_entity_t* localPlayer = (gEngfuncs.GetLocalPlayer ? gEngfuncs.GetLocalPlayer() : nullptr);
    if (localPlayer != nullptr && g_NitroApi != nullptr)
    {
        auto* clientData = g_NitroApi->GetClientData();
        if (clientData != nullptr && localPlayer->index >= 1 && localPlayer->index <= 32)
        {
            int team = clientData->g_PlayerExtraInfo[localPlayer->index].teamnumber;
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

    AddChatMessage(0, prefix, (myName && *myName) ? myName : "Me", message, r, g, b);
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

        // 2. Frosted Obsidian Glass Body
        DrawBox(cardX, currentMsgBottomY, cardW, lineH, 12, 16, 26, (alpha * 220) / 255);

        // 3. Top Hairline Specular Highlight
        DrawBox(cardX, currentMsgBottomY, cardW, 1, 255, 255, 255, (alpha * 35) / 255);

        // 4. Dynamic Left Team-Color Neon Pillar
        DrawBox(cardX, currentMsgBottomY, 3, lineH,
                static_cast<int>(m.r * 255), static_cast<int>(m.g * 255), static_cast<int>(m.b * 255), alpha);

        // 5. Dynamic Kinetic Laser Progress Line at base of card
        if (!IsOpen())
        {
            float remaining = std::clamp(1.0f - static_cast<float>(age / kMessageLifetime), 0.0f, 1.0f);
            int progW = static_cast<int>((cardW - 4) * remaining);
            if (progW > 0)
            {
                DrawBox(cardX + 2, currentMsgBottomY + lineH - 2, progW, 1,
                        static_cast<int>(m.r * 255), static_cast<int>(m.g * 255), static_cast<int>(m.b * 255), (alpha * 160) / 255);
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

            // Sleek badge pill background
            DrawBox(posX, badgeY, badgeW, badgeH,
                    static_cast<int>(m.r * 80), static_cast<int>(m.g * 80), static_cast<int>(m.b * 80), (alpha * 180) / 255);
            DrawBox(posX, badgeY, badgeW, 1,
                    static_cast<int>(m.r * 255), static_cast<int>(m.g * 255), static_cast<int>(m.b * 255), (alpha * 200) / 255);

            DrawTextWithShadow(posX + badgePad, textY, m.prefix.c_str(), 0.90f, 0.95f, 1.0f);
            posX += badgeW + 8;
        }

        // 7. Sender Name in Team Color with Drop-Shadow
        if (!m.sender.empty())
        {
            DrawTextWithShadow(posX, textY, m.sender.c_str(), m.r, m.g, m.b);
            posX += senderW + 2;
            DrawTextWithShadow(posX, textY, ":", 0.85f, 0.88f, 0.92f);
            posX += 8;
        }

        // 8. Message Content in Crystal Diamond White with Drop-Shadow
        DrawTextWithShadow(posX, textY, m.text.c_str(), 0.96f, 0.97f, 1.0f);

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

    // 5. Action Badges (SEND & CANCEL)
    int btnW = static_cast<int>(78.0f * scale + 0.5f);
    int btnH = m_barH - 10;
    int btnY = m_barY + 5;

    m_cancelBtnX = m_barX + m_barW - btnW - 6;
    m_cancelBtnY = btnY;
    m_cancelBtnW = btnW;
    m_cancelBtnH = btnH;

    m_sendBtnX = m_cancelBtnX - btnW - 6;
    m_sendBtnY = btnY;
    m_sendBtnW = btnW;
    m_sendBtnH = btnH;

    // SEND Action Indicator:
    DrawBox(m_sendBtnX, m_sendBtnY, m_sendBtnW, m_sendBtnH, 0, 130, 180, 180);
    DrawBox(m_sendBtnX, m_sendBtnY, m_sendBtnW, 1, 0, 220, 255, pulseAlpha);
    DrawTextWithShadow(m_sendBtnX + 10, m_sendBtnY + (btnH - 13) / 2, "SEND [L]", 0.3f, 0.95f, 1.0f);

    // CANCEL Action Indicator:
    DrawBox(m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, m_cancelBtnH, 55, 60, 70, 170);
    DrawBox(m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, 1, 220, 80, 95, 120);
    DrawTextWithShadow(m_cancelBtnX + 6, m_cancelBtnY + (btnH - 13) / 2, "CANCEL [R]", 0.95f, 0.55f, 0.60f);

    // 6. Mode Badge with breathing glow:
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

    // 7. Player Identity Tag:
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

    // 8. Input Text Field with smooth horizontal auto-scroll
    int inputMaxW = m_sendBtnX - inputX - 12;
    const char* pDrawText = m_buffer.c_str();
    std::string visibleBuffer;
    int textW = 0, textH = 0;

    if (m_buffer.empty())
    {
        DrawTextWithShadow(inputX, inputY, "Type your message... [Left-Click/Enter to Send, Right-Click to Cancel]", 0.55f, 0.60f, 0.68f);
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

    // 9. Pulsing Glowing Cursor '|'
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

    // 10. Dynamic Character Progress Micro-Bar under Input Field
    float charRatio = std::clamp(static_cast<float>(m_buffer.size()) / 120.0f, 0.0f, 1.0f);
    int charBarW = static_cast<int>((m_barW - 12) * charRatio);
    int charBarR = (m_buffer.size() > 100) ? 255 : (m_buffer.size() > 70 ? 255 : (isTeam ? 46 : 0));
    int charBarG = (m_buffer.size() > 100) ? 65  : (m_buffer.size() > 70 ? 190 : (isTeam ? 213 : 220));
    int charBarB = (m_buffer.size() > 100) ? 65  : (m_buffer.size() > 70 ? 40  : (isTeam ? 115 : 255));
    if (charBarW > 0)
    {
        DrawBox(m_barX + 6, m_barY + m_barH - 2, charBarW, 1, charBarR, charBarG, charBarB, pulseAlpha);
    }

    // 11. Modern Dynamic Helper Ribbon Below
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
        "[Left-Click / Enter] Send   *   [Right-Click / Esc] Cancel   *   [Tab] Switch Mode   *   [Ctrl+V] Paste",
        0.75f, 0.80f, 0.88f);

    float countR = (m_buffer.size() > 100) ? 1.0f : 0.50f;
    float countG = (m_buffer.size() > 100) ? 0.35f : 0.75f;
    float countB = (m_buffer.size() > 100) ? 0.35f : 0.90f;
    DrawTextWithShadow(m_barX + m_barW - countW - 10, helpY + 3, countBuf, countR, countG, countB);
}
