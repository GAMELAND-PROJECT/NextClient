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
        if (gEngfuncs.pfnFillRGBA != nullptr && w > 0 && h > 0)
            gEngfuncs.pfnFillRGBA(x, y, w, h, r, g, b, a);
    }

    void DrawText(int x, int y, const char* str, float r, float g, float b)
    {
        if (gEngfuncs.pfnDrawSetTextColor != nullptr)
            gEngfuncs.pfnDrawSetTextColor(r, g, b);
        if (gEngfuncs.pfnDrawConsoleString != nullptr && str != nullptr)
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

    bool PointInRect(int px, int py, int rx, int ry, int rw, int rh)
    {
        return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
    }

    void GetGameMousePos(int& outX, int& outY)
    {
        outX = -1;
        outY = -1;

        int engX = 0, engY = 0;
        if (gEngfuncs.GetMousePosition != nullptr)
        {
            gEngfuncs.GetMousePosition(&engX, &engY);
            if (engX > 0 || engY > 0)
            {
                outX = engX;
                outY = engY;
                return;
            }
        }

#ifdef _WIN32
        HWND hWnd = FindWindowA("Valve001", nullptr);
        if (!hWnd) hWnd = GetActiveWindow();
        if (hWnd)
        {
            POINT pt;
            if (GetCursorPos(&pt))
            {
                ScreenToClient(hWnd, &pt);
                outX = pt.x;
                outY = pt.y;
            }
        }
#endif
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

    // Free the mouse cursor for interactive clicking
    IN_DeactivateMouse();
}

void ModernChat::Close()
{
    if (m_mode == ModernChatMode::Closed)
        return;

    m_mode = ModernChatMode::Closed;
    m_buffer.clear();

    // Re-lock mouse to crosshair for normal gameplay
    IN_ActivateMouse();
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

int ModernChat::HandleKey(int down, int keynum, const char* /*pszCurrentBinding*/)
{
    if (!IsOpen())
        return 1;

    if (down == 0)
        return 0; // Consume key releases

    // 1. Mouse Clicks
    constexpr int kMouse1 = 241;
    constexpr int kMouse2 = 242;

    if (keynum == kMouse1) // Left Click
    {
        int mx = -1, my = -1;
        GetGameMousePos(mx, my);

        if (PointInRect(mx, my, m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, m_cancelBtnH))
        {
            Cancel();
            return 0;
        }

        if (PointInRect(mx, my, m_sendBtnX, m_sendBtnY, m_sendBtnW, m_sendBtnH))
        {
            Send();
            return 0;
        }

        // Quick left-click send as requested
        Send();
        return 0;
    }

    if (keynum == kMouse2) // Right Click
    {
        // Right-click cancels as requested
        Cancel();
        return 0;
    }

    if (keynum >= 243 && keynum <= 245)
    {
        return 0;
    }

    // 2. Keyboard Actions
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

    // 3. Printable character typing
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
    if (!IsOpen())
        return;

    float scale = static_cast<float>(scrH) / 600.0f;
    if (scale < 1.0f) scale = 1.0f;

    m_barW = std::min(scrW - 48, static_cast<int>(540.0f * scale + 0.5f));
    m_barH = static_cast<int>(38.0f * scale + 0.5f);
    m_barX = 24;
    m_barY = scrH - static_cast<int>(180.0f * scale + 0.5f);

    // 1. Soft Outer Drop-Shadow
    DrawBox(m_barX - 2, m_barY - 2, m_barW + 4, m_barH + 4, 0, 0, 0, 150);

    // 2. Obsidian Glass Body
    DrawBox(m_barX, m_barY, m_barW, m_barH, 14, 18, 26, 235);

    // 3. Mode-Specific Header Accent Line
    const bool isTeam = (m_mode == ModernChatMode::SayTeam);
    if (isTeam)
    {
        DrawBox(m_barX, m_barY, m_barW, 2, 46, 213, 115, 230); // Emerald Green
    }
    else
    {
        DrawBox(m_barX, m_barY, m_barW, 2, 0, 220, 255, 230); // Neon Cyan
    }

    // 4. Subtle Border Hairlines
    DrawBox(m_barX, m_barY + m_barH - 1, m_barW, 1, 255, 255, 255, 25);
    DrawBox(m_barX, m_barY, 1, m_barH, 255, 255, 255, 25);
    DrawBox(m_barX + m_barW - 1, m_barY, 1, m_barH, 255, 255, 255, 25);

    // 5. Interactive Buttons (SEND & CANCEL)
    int btnW = static_cast<int>(74.0f * scale + 0.5f);
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

    int mx = -1, my = -1;
    GetGameMousePos(mx, my);
    bool sendHover = PointInRect(mx, my, m_sendBtnX, m_sendBtnY, m_sendBtnW, m_sendBtnH);
    bool cancelHover = PointInRect(mx, my, m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, m_cancelBtnH);

    // SEND Button:
    if (sendHover)
    {
        DrawBox(m_sendBtnX, m_sendBtnY, m_sendBtnW, m_sendBtnH, 0, 185, 245, 240);
        DrawBox(m_sendBtnX, m_sendBtnY, m_sendBtnW, 1, 255, 255, 255, 255);
        DrawText(m_sendBtnX + 14, m_sendBtnY + (btnH - 13) / 2, "SEND [L]", 1.0f, 1.0f, 1.0f);
    }
    else
    {
        DrawBox(m_sendBtnX, m_sendBtnY, m_sendBtnW, m_sendBtnH, 0, 110, 150, 160);
        DrawBox(m_sendBtnX, m_sendBtnY, m_sendBtnW, 1, 0, 220, 255, 140);
        DrawText(m_sendBtnX + 14, m_sendBtnY + (btnH - 13) / 2, "SEND [L]", 0.3f, 0.95f, 1.0f);
    }

    // CANCEL Button:
    if (cancelHover)
    {
        DrawBox(m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, m_cancelBtnH, 225, 45, 65, 240);
        DrawBox(m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, 1, 255, 255, 255, 255);
        DrawText(m_cancelBtnX + 8, m_cancelBtnY + (btnH - 13) / 2, "CANCEL [R]", 1.0f, 1.0f, 1.0f);
    }
    else
    {
        DrawBox(m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, m_cancelBtnH, 55, 60, 70, 160);
        DrawBox(m_cancelBtnX, m_cancelBtnY, m_cancelBtnW, 1, 220, 80, 95, 110);
        DrawText(m_cancelBtnX + 8, m_cancelBtnY + (btnH - 13) / 2, "CANCEL [R]", 0.95f, 0.55f, 0.60f);
    }

    // 6. Mode Badge:
    int badgeW = static_cast<int>(80.0f * scale + 0.5f);
    int badgeH = m_barH - 10;
    int badgeX = m_barX + 6;
    int badgeY = m_barY + 5;

    if (isTeam)
    {
        DrawBox(badgeX, badgeY, badgeW, badgeH, 25, 110, 60, 200);
        DrawBox(badgeX, badgeY, badgeW, 1, 46, 213, 115, 230);
        DrawText(badgeX + 10, badgeY + (badgeH - 13) / 2, "[ TEAM ]", 0.35f, 1.0f, 0.55f);
    }
    else
    {
        DrawBox(badgeX, badgeY, badgeW, badgeH, 0, 105, 145, 200);
        DrawBox(badgeX, badgeY, badgeW, 1, 0, 220, 255, 230);
        DrawText(badgeX + 14, badgeY + (badgeH - 13) / 2, "[ ALL ]", 0.3f, 0.95f, 1.0f);
    }

    // 7. Input Field & Text
    int inputX = badgeX + badgeW + 10;
    int inputY = m_barY + (m_barH - 13) / 2;
    int inputMaxW = m_sendBtnX - inputX - 10;

    const char* pDrawText = m_buffer.c_str();
    std::string visibleBuffer;
    int textW = 0, textH = 0;

    if (m_buffer.empty())
    {
        DrawText(inputX, inputY, "Type message...  (L-Click / Enter to Send)", 0.55f, 0.60f, 0.68f);
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
        DrawText(inputX, inputY, pDrawText, 0.98f, 0.98f, 1.0f);
    }

    // 8. Blinking Cursor '|'
    double curTime = (gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0);
    bool blink = (static_cast<int>(curTime * 2.8) % 2) == 0;
    if (blink)
    {
        int curX = inputX + textW + 2;
        int curY = m_barY + 8;
        DrawBox(curX, curY, 2, m_barH - 16, 0, 220, 255, 230);
    }

    // 9. Modern Helper Info Ribbon Below
    int helpY = m_barY + m_barH + 4;
    int helpH = 18;
    DrawBox(m_barX, helpY, m_barW, helpH, 10, 14, 20, 200);
    DrawBox(m_barX, helpY, m_barW, 1, 255, 255, 255, 20);
    DrawText(m_barX + 12, helpY + 3,
        "[Enter / L-Click] Send   *   [Esc / R-Click] Cancel   *   [Tab] Switch All/Team   *   [Ctrl+V] Paste",
        0.75f, 0.80f, 0.88f);
}
