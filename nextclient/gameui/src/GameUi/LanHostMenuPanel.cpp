#include "GameUi.h"
#include "LanHostMenuPanel.h"
#include "GameConsoleDialog.h"
#include <vgui/ISurfaceNext.h>
#include <algorithm>
#include <format>

#undef PostMessage

CLanHostMenuPanel::CLanHostMenuPanel(vgui2::Panel* parent, CGameConsoleDialog* pConsoleDialog)
    : Panel(parent, "LanHostMenu"), m_pConsoleDialog(pConsoleDialog)
{
    SetMouseInputEnabled(true);
    SetKeyBoardInputEnabled(false);
    SetPaintBackgroundEnabled(false);

    SyncCvarsFromEngine();
}

CLanHostMenuPanel::~CLanHostMenuPanel()
{
    if (m_texture)
    {
        vgui2::surface()->DeleteTextureByID(m_texture);
        m_texture = 0;
    }
}

void CLanHostMenuPanel::SyncCvarsFromEngine()
{
    if (!engine)
        return;

    bool prevFf = m_bFriendlyFire;
    int prevFt = m_iFreezeTime;
    float prevRt = m_fRoundTime;
    int prevSm = m_iStartMoney;

    m_bFriendlyFire = (engine->pfnGetCvarFloat("mp_friendlyfire") > 0.0f);
    m_iFreezeTime = static_cast<int>(engine->pfnGetCvarFloat("mp_freezetime"));
    m_fRoundTime = engine->pfnGetCvarFloat("mp_roundtime");
    m_iStartMoney = static_cast<int>(engine->pfnGetCvarFloat("mp_startmoney"));

    if (m_bFriendlyFire != prevFf || m_iFreezeTime != prevFt ||
        std::abs(m_fRoundTime - prevRt) > 0.01f || m_iStartMoney != prevSm)
    {
        m_needsRedraw = true;
    }
}

void CLanHostMenuPanel::OnThink()
{
    BaseClass::OnThink();
    SyncCvarsFromEngine();
}

void CLanHostMenuPanel::OnCursorMoved(int x, int y)
{
    ButtonHitbox* pFound = FindHitbox(x, y);
    ButtonId newHover = pFound ? pFound->id : BTN_NONE;

    if (newHover != m_hoveredId)
    {
        m_hoveredId = newHover;
        m_needsRedraw = true;
        Repaint();
    }
}

void CLanHostMenuPanel::OnCursorEntered()
{
}

void CLanHostMenuPanel::OnCursorExited()
{
    if (m_hoveredId != BTN_NONE)
    {
        m_hoveredId = BTN_NONE;
        m_needsRedraw = true;
        Repaint();
    }
}

void CLanHostMenuPanel::OnMousePressed(vgui2::MouseCode code)
{
    if (code == vgui2::MOUSE_LEFT)
    {
        m_pressedId = m_hoveredId;
        m_needsRedraw = true;
        Repaint();
    }
}

void CLanHostMenuPanel::OnMouseReleased(vgui2::MouseCode code)
{
    if (code == vgui2::MOUSE_LEFT)
    {
        if (m_pressedId != BTN_NONE && m_pressedId == m_hoveredId)
        {
            ExecuteAction(m_pressedId);
        }
        m_pressedId = BTN_NONE;
        m_needsRedraw = true;
        Repaint();
    }
}

CLanHostMenuPanel::ButtonHitbox* CLanHostMenuPanel::FindHitbox(int x, int y)
{
    POINT pt{ x, y };
    for (auto& hb : m_hitboxes)
    {
        if (PtInRect(&hb.rect, pt))
            return &hb;
    }
    return nullptr;
}

void CLanHostMenuPanel::BuildHitboxes(int width, int height)
{
    m_hitboxes.clear();

    // 1. Header Button: Switch to Console View
    RECT rcConsoleTab{ width - 215, 8, width - 12, 34 };
    m_hitboxes.push_back({
        BTN_TAB_CONSOLE,
        rcConsoleTab,
        "",
        L"کنسول متنی (Console Logs)",
        L"مشاهده لاگ‌ها و دستورات",
        false,
        false,
        RGB(104, 216, 193)
    });

    // 2. Section: Game Modes (5 buttons in a single clean row)
    const int modesTop = 64;
    const int modesBottom = 126;
    const int leftPad = 12;
    const int rightPad = 12;
    const int availableW = width - leftPad - rightPad;
    const int gap = 8;
    const int btnW = (availableW - (gap * 4)) / 5;

    struct ModeDef {
        ButtonId id;
        std::string modeKey;
        std::wstring title;
        std::wstring subtitle;
        COLORREF accent;
    };

    static const ModeDef modes[] = {
        { BTN_MODE_WARMUP,  "warm", L"WARMUP",     L"تمرینی و وارم‌آپ", RGB(245, 185, 60) },
        { BTN_MODE_MATCH,   "mix",  L"MATCH 5v5",  L"مسابقه رسمی",     RGB(60, 200, 245) },
        { BTN_MODE_1V1,     "1v1",  L"1v1 DUEL",   L"دوئل تک‌به‌تک",    RGB(185, 110, 255) },
        { BTN_MODE_LIVE,    "LIVE", L"LIVE MATCH", L"شروع زنده بازی",   RGB(255, 75, 75) },
        { BTN_MODE_RESTART, "r",    L"RESTART",    L"ری‌استارت راند",   RGB(80, 230, 140) }
    };

    int curX = leftPad;
    for (int i = 0; i < 5; ++i)
    {
        RECT rcBtn{ curX, modesTop, curX + btnW, modesBottom };
        bool isActive = (m_currentMode == modes[i].modeKey);
        m_hitboxes.push_back({
            modes[i].id,
            rcBtn,
            modes[i].modeKey,
            modes[i].title,
            modes[i].subtitle,
            false,
            isActive,
            modes[i].accent
        });
        curX += btnW + gap;
    }

    // 3. Section: Friendly Fire & Round Time (Row A)
    const int rowATop = 158;
    const int rowABottom = 236;
    const int halfW = (availableW - gap) / 2;

    // Friendly Fire Toggle
    RECT rcFF{ leftPad + 8, rowATop + 28, leftPad + halfW - 8, rowABottom - 10 };
    m_hitboxes.push_back({
        BTN_TOGGLE_FF,
        rcFF,
        "toggle_ff",
        m_bFriendlyFire ? L"✔  تیر به خودی: فعال (ON)" : L"✖  تیر به خودی: غیرفعال (OFF)",
        m_bFriendlyFire ? L"کلیک کنید تا خاموش شود" : L"کلیک کنید تا روشن شود",
        true,
        m_bFriendlyFire,
        m_bFriendlyFire ? RGB(60, 220, 120) : RGB(240, 70, 70)
    });

    // Round Time Chips
    const int rtAreaX = leftPad + halfW + gap + 8;
    const int rtAreaW = halfW - 16;
    const int rtChipW = (rtAreaW - 16) / 5;
    const int rtChipY = rowATop + 28;
    const int rtChipH = 34;

    struct RoundDef { ButtonId id; float val; const wchar_t* label; };
    static const RoundDef roundDefs[] = {
        { BTN_ROUND_100, 1.00f, L"1:00" },
        { BTN_ROUND_145, 1.45f, L"1:45" },
        { BTN_ROUND_175, 1.75f, L"1:75" },
        { BTN_ROUND_200, 2.00f, L"2:00" },
        { BTN_ROUND_300, 3.00f, L"3:00" }
    };

    for (int i = 0; i < 5; ++i)
    {
        RECT rcChip{ rtAreaX + i * (rtChipW + 4), rtChipY, rtAreaX + i * (rtChipW + 4) + rtChipW, rtChipY + rtChipH };
        bool isActive = std::abs(m_fRoundTime - roundDefs[i].val) < 0.05f;
        m_hitboxes.push_back({
            roundDefs[i].id,
            rcChip,
            "",
            roundDefs[i].label,
            L"دقیقه",
            false,
            isActive,
            RGB(60, 200, 245)
        });
    }

    // 4. Section: Freeze Time Steppers & Chips (Row B)
    const int rowBTop = 248;
    const int rowBBottom = 328;

    // Steppers [-] and [+]
    RECT rcMinus{ leftPad + 12, rowBTop + 30, leftPad + 48, rowBTop + 66 };
    m_hitboxes.push_back({
        BTN_FREEZE_MINUS,
        rcMinus,
        "-1",
        L"[-]",
        L"کاهش",
        false,
        false,
        RGB(245, 185, 60)
    });

    RECT rcPlus{ leftPad + 54, rowBTop + 30, leftPad + 90, rowBTop + 66 };
    m_hitboxes.push_back({
        BTN_FREEZE_PLUS,
        rcPlus,
        "+1",
        L"[+]",
        L"افزایش",
        false,
        false,
        RGB(245, 185, 60)
    });

    // Preset Chips: 0s, 3s, 5s, 10s, 12s, 15s
    const int ftChipsStartX = leftPad + 104;
    const int ftChipsAvailableW = width - rightPad - ftChipsStartX - 12;
    const int ftChipW = (ftChipsAvailableW - (5 * 6)) / 6;

    struct FreezeDef { ButtonId id; int sec; const wchar_t* label; };
    static const FreezeDef freezeDefs[] = {
        { BTN_FREEZE_0,   0, L"0s" },
        { BTN_FREEZE_3,   3, L"3s" },
        { BTN_FREEZE_5,   5, L"5s" },
        { BTN_FREEZE_10, 10, L"10s" },
        { BTN_FREEZE_12, 12, L"12s" },
        { BTN_FREEZE_15, 15, L"15s" }
    };

    for (int i = 0; i < 6; ++i)
    {
        RECT rcFtChip{
            ftChipsStartX + i * (ftChipW + 6),
            rowBTop + 30,
            ftChipsStartX + i * (ftChipW + 6) + ftChipW,
            rowBTop + 66
        };
        bool isActive = (m_iFreezeTime == freezeDefs[i].sec);
        m_hitboxes.push_back({
            freezeDefs[i].id,
            rcFtChip,
            "",
            freezeDefs[i].label,
            freezeDefs[i].sec == 12 ? L"مسابقه" : (freezeDefs[i].sec == 0 ? L"سریع" : L"ثانیه"),
            false,
            isActive,
            RGB(80, 230, 140)
        });
    }

    // 5. Section: Start Money (Row C)
    const int rowCTop = 338;
    const int rowCBottom = 402;
    const int smChipsStartX = leftPad + 12;
    const int smChipsAvailableW = width - rightPad - smChipsStartX - 12;
    const int smChipW = (smChipsAvailableW - (2 * 12)) / 3;

    struct MoneyDef { ButtonId id; int money; const wchar_t* title; const wchar_t* sub; };
    static const MoneyDef moneyDefs[] = {
        { BTN_MONEY_800, 800, L"$800", L"پیستول / مسابقه رسمی" },
        { BTN_MONEY_16K, 16000, L"$16,000", L"حداکثر سقف مسابقه" },
        { BTN_MONEY_MAX, 999999, L"نامحدود ($999,999)", L"خرید آزاد تمرینی" }
    };

    for (int i = 0; i < 3; ++i)
    {
        RECT rcSmChip{
            smChipsStartX + i * (smChipW + 12),
            rowCTop + 24,
            smChipsStartX + i * (smChipW + 12) + smChipW,
            rowCTop + 56
        };
        bool isActive = (m_iStartMoney == moneyDefs[i].money) || (moneyDefs[i].money > 16000 && m_iStartMoney > 16000);
        m_hitboxes.push_back({
            moneyDefs[i].id,
            rcSmChip,
            "",
            moneyDefs[i].title,
            moneyDefs[i].sub,
            false,
            isActive,
            RGB(245, 201, 112)
        });
    }
}

void CLanHostMenuPanel::ExecuteAction(ButtonId id)
{
    // Play pleasant tactical UI feedback sound
    vgui2::surface()->PlaySound("common/wpn_select.wav");

    switch (id)
    {
    case BTN_TAB_CONSOLE:
        if (m_pConsoleDialog)
            m_pConsoleDialog->ShowLanHostGuide(false);
        break;

    case BTN_MODE_WARMUP:
        SetMode("warm");
        break;

    case BTN_MODE_MATCH:
        SetMode("mix");
        break;

    case BTN_MODE_1V1:
        SetMode("1v1");
        break;

    case BTN_MODE_LIVE:
        if (engine)
        {
            engine->pfnClientCmd("LIVE\n");
            if (m_pConsoleDialog)
            {
                m_pConsoleDialog->PrintHostStatusCard(
                    "شمارش معکوس و آغاز زنده مسابقه (LIVE 3s)",
                    m_currentMode.c_str(),
                    m_bFriendlyFire,
                    m_iFreezeTime,
                    m_fRoundTime,
                    m_iStartMoney
                );
            }
        }
        break;

    case BTN_MODE_RESTART:
        if (engine)
        {
            engine->pfnClientCmd("sv_restartround 1\n");
            if (m_pConsoleDialog)
            {
                m_pConsoleDialog->PrintHostStatusCard(
                    "شروع مجدد فوری راند (Round Restart)",
                    m_currentMode.c_str(),
                    m_bFriendlyFire,
                    m_iFreezeTime,
                    m_fRoundTime,
                    m_iStartMoney
                );
            }
        }
        break;

    case BTN_TOGGLE_FF:
        ToggleFriendlyFire();
        break;

    case BTN_FREEZE_MINUS:
        AdjustFreezeTime(-1);
        break;

    case BTN_FREEZE_PLUS:
        AdjustFreezeTime(+1);
        break;

    case BTN_FREEZE_0:   SetFreezeTime(0); break;
    case BTN_FREEZE_3:   SetFreezeTime(3); break;
    case BTN_FREEZE_5:   SetFreezeTime(5); break;
    case BTN_FREEZE_10:  SetFreezeTime(10); break;
    case BTN_FREEZE_12:  SetFreezeTime(12); break;
    case BTN_FREEZE_15:  SetFreezeTime(15); break;

    case BTN_ROUND_100: SetRoundTime(1.00f); break;
    case BTN_ROUND_145: SetRoundTime(1.45f); break;
    case BTN_ROUND_175: SetRoundTime(1.75f); break;
    case BTN_ROUND_200: SetRoundTime(2.00f); break;
    case BTN_ROUND_300: SetRoundTime(3.00f); break;

    case BTN_MONEY_800: SetStartMoney(800); break;
    case BTN_MONEY_16K: SetStartMoney(16000); break;
    case BTN_MONEY_MAX: SetStartMoney(999999); break;

    default:
        break;
    }
}

void CLanHostMenuPanel::SetMode(const char* modeName)
{
    if (!engine)
        return;

    m_currentMode = modeName;
    std::string cmd = std::format("{}\n", modeName);
    engine->pfnClientCmd(cmd.c_str());

    if (m_currentMode == "warm")
    {
        m_bFriendlyFire = false;
        m_iFreezeTime = 0;
        m_fRoundTime = 5.0f;
        m_iStartMoney = 999999;
        if (m_pConsoleDialog)
            m_pConsoleDialog->PrintHostStatusCard("حالت تمرینی و گرم‌کردن فعال شد (WARMUP)", "تمرینی (WARMUP)", false, 0, 5.0f, 999999);
    }
    else if (m_currentMode == "mix")
    {
        m_bFriendlyFire = true;
        m_iFreezeTime = 12;
        m_fRoundTime = 1.75f;
        m_iStartMoney = 800;
        if (m_pConsoleDialog)
            m_pConsoleDialog->PrintHostStatusCard("مسابقه رسمی ۵ به ۵ فعال شد (MATCH 5v5)", "مسابقه رسمی ۵ به ۵ (MATCH)", true, 12, 1.75f, 800);
    }
    else if (m_currentMode == "1v1")
    {
        m_bFriendlyFire = false;
        m_iFreezeTime = 0;
        m_fRoundTime = 0.5f;
        m_iStartMoney = 16000;
        if (m_pConsoleDialog)
            m_pConsoleDialog->PrintHostStatusCard("دوئل ۱ به ۱ فعال شد (1v1 DUEL)", "دوئل تک‌به‌تک (1v1 DUEL)", false, 0, 0.5f, 16000);
    }

    m_needsRedraw = true;
    Repaint();
}

void CLanHostMenuPanel::ToggleFriendlyFire()
{
    if (!engine)
        return;

    m_bFriendlyFire = !m_bFriendlyFire;
    std::string cmd = m_bFriendlyFire ? "ff1\n" : "ff0\n";
    engine->pfnClientCmd(cmd.c_str());

    if (m_pConsoleDialog)
    {
        m_pConsoleDialog->PrintHostStatusCard(
            m_bFriendlyFire ? "تیر به خودی فعال شد (Friendly Fire ON)" : "تیر به خودی خاموش شد (Friendly Fire OFF)",
            m_currentMode.c_str(),
            m_bFriendlyFire,
            m_iFreezeTime,
            m_fRoundTime,
            m_iStartMoney
        );
    }

    m_needsRedraw = true;
    Repaint();
}

void CLanHostMenuPanel::SetFreezeTime(int seconds)
{
    if (!engine)
        return;

    seconds = std::clamp(seconds, 0, 15);
    m_iFreezeTime = seconds;

    std::string cmd = std::format("fr{}\n", seconds);
    engine->pfnClientCmd(cmd.c_str());

    if (m_pConsoleDialog)
    {
        m_pConsoleDialog->PrintHostStatusCard(
            "وقت اولیه راند تغییر یافت (Freeze Time Changed)",
            m_currentMode.c_str(),
            m_bFriendlyFire,
            m_iFreezeTime,
            m_fRoundTime,
            m_iStartMoney
        );
    }

    m_needsRedraw = true;
    Repaint();
}

void CLanHostMenuPanel::AdjustFreezeTime(int delta)
{
    SetFreezeTime(m_iFreezeTime + delta);
}

void CLanHostMenuPanel::SetRoundTime(float minutes)
{
    if (!engine)
        return;

    m_fRoundTime = minutes;
    std::string cmd = std::format("mp_roundtime {:.2f}; wait; say === ROUND TIME {:.2f} MIN ===\n", minutes, minutes);
    engine->pfnClientCmd(cmd.c_str());

    if (m_pConsoleDialog)
    {
        m_pConsoleDialog->PrintHostStatusCard(
            "زمان هر راند تغییر یافت (Round Time Changed)",
            m_currentMode.c_str(),
            m_bFriendlyFire,
            m_iFreezeTime,
            m_fRoundTime,
            m_iStartMoney
        );
    }

    m_needsRedraw = true;
    Repaint();
}

void CLanHostMenuPanel::SetStartMoney(int money)
{
    if (!engine)
        return;

    m_iStartMoney = money;
    std::string cmd = std::format("mp_startmoney {}; wait; say === START MONEY ${} ===\n", money, money);
    engine->pfnClientCmd(cmd.c_str());

    if (m_pConsoleDialog)
    {
        m_pConsoleDialog->PrintHostStatusCard(
            "سرمایه اولیه تغییر یافت (Start Money Changed)",
            m_currentMode.c_str(),
            m_bFriendlyFire,
            m_iFreezeTime,
            m_fRoundTime,
            m_iStartMoney
        );
    }

    m_needsRedraw = true;
    Repaint();
}

std::vector<unsigned char> CLanHostMenuPanel::RenderHostMenuBitmap(int width, int height)
{
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return {};

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return {};

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);

    HFONT fontTitle = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");

    HFONT fontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");

    HFONT fontNormal = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");

    HFONT fontSmall = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");

    if (!bitmap || !fontTitle || !fontBold || !fontNormal || !fontSmall)
    {
        if (bitmap) DeleteObject(bitmap);
        if (fontTitle) DeleteObject(fontTitle);
        if (fontBold) DeleteObject(fontBold);
        if (fontNormal) DeleteObject(fontNormal);
        if (fontSmall) DeleteObject(fontSmall);
        DeleteDC(dc);
        return {};
    }

    HGDIOBJ prevBmp = SelectObject(dc, bitmap);
    HGDIOBJ prevFont = SelectObject(dc, fontTitle);

    // 1. Overall Dark Background
    RECT rcAll{ 0, 0, width, height };
    SetDCBrushColor(dc, RGB(16, 20, 26));
    FillRect(dc, &rcAll, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    // 2. Header Bar
    RECT rcHeader{ 0, 0, width, 44 };
    SetDCBrushColor(dc, RGB(22, 28, 38));
    FillRect(dc, &rcHeader, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    SetBkMode(dc, TRANSPARENT);

    // Header Title
    SetTextColor(dc, RGB(245, 201, 112));
    SelectObject(dc, fontTitle);
    RECT rcTitleText{ 16, 4, width - 230, 24 };
    DrawTextW(dc, L"🎮  GAMELAND MATCH & HOST MANAGER", -1, &rcTitleText, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Header Subtitle (Persian RTL)
    SetTextColor(dc, RGB(104, 216, 193));
    SelectObject(dc, fontNormal);
    RECT rcSubText{ 16, 23, width - 230, 41 };
    DrawTextW(dc, L"کنترل‌پنل مدیریت مسابقه و هاست لن | اجرای سریع حالت‌ها و قوانین", -1, &rcSubText,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // 3. Section 1 Label
    SelectObject(dc, fontBold);
    SetTextColor(dc, RGB(230, 235, 240));
    RECT rcSec1{ 16, 47, width - 16, 62 };
    DrawTextW(dc, L"حالت‌های اصلی بازی و مسابقه (Select Match Mode):", -1, &rcSec1,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Section 2 Labels
    RECT rcSec2{ 16, 137, width - 16, 154 };
    DrawTextW(dc, L"قوانین و تنظیمات لحظه‌ای سرور (Live Match Settings):", -1, &rcSec2,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Draw Card Backgrounds for Sections
    const int availableW = width - 24;
    const int gap = 8;
    const int halfW = (availableW - gap) / 2;

    // Card Friendly Fire
    RECT rcCardFF{ 12, 156, 12 + halfW, 238 };
    SetDCBrushColor(dc, RGB(22, 28, 36));
    FillRect(dc, &rcCardFF, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    SelectObject(dc, fontSmall);
    SetTextColor(dc, RGB(160, 175, 190));
    RECT rcLblFF{ 20, 160, 12 + halfW - 8, 176 };
    DrawTextW(dc, L"وضعیت آسیب به هم‌تیمی‌ها (Friendly Fire):", -1, &rcLblFF,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Card Round Time
    RECT rcCardRT{ 12 + halfW + gap, 156, width - 12, 238 };
    FillRect(dc, &rcCardRT, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    RECT rcLblRT{ 12 + halfW + gap + 8, 160, width - 20, 176 };
    wchar_t rtBuf[64];
    swprintf_s(rtBuf, L"زمان هر راند: [%.2f دقیقه] (Round Time)", m_fRoundTime);
    DrawTextW(dc, rtBuf, -1, &rcLblRT,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Card Freeze Time
    RECT rcCardFT{ 12, 246, width - 12, 330 };
    FillRect(dc, &rcCardFT, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    RECT rcLblFT{ 20, 250, width - 20, 268 };
    wchar_t ftBuf[64];
    swprintf_s(ftBuf, L"وقت اولیه ابتدای راند: [%d ثانیه] (Freeze Time)", m_iFreezeTime);
    DrawTextW(dc, ftBuf, -1, &rcLblFT,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Card Start Money
    RECT rcCardSM{ 12, 338, width - 12, 404 };
    FillRect(dc, &rcCardSM, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    RECT rcLblSM{ 20, 342, width - 20, 358 };
    wchar_t smBuf[64];
    swprintf_s(smBuf, L"سرمایه اولیه بازیکنان: [$%d] (Start Money)", m_iStartMoney);
    DrawTextW(dc, smBuf, -1, &rcLblSM,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Rebuild hitboxes to current dimensions
    BuildHitboxes(width, height);

    // 4. Draw All Buttons & Hitboxes
    for (const auto& btn : m_hitboxes)
    {
        bool isHovered = (m_hoveredId == btn.id);
        bool isPressed = (m_pressedId == btn.id);

        COLORREF bgCol = RGB(26, 33, 44);
        if (btn.isActive)
            bgCol = RGB(32, 44, 60);
        if (isHovered)
            bgCol = RGB(38, 52, 70);
        if (isPressed)
            bgCol = RGB(20, 26, 36);

        // Special fill for friendly fire toggle
        if (btn.id == BTN_TOGGLE_FF)
        {
            if (m_bFriendlyFire)
                bgCol = isHovered ? RGB(20, 58, 36) : RGB(14, 44, 26);
            else
                bgCol = isHovered ? RGB(58, 20, 26) : RGB(44, 14, 18);
        }

        // Draw button body
        SetDCBrushColor(dc, bgCol);
        FillRect(dc, &btn.rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

        // Draw Border
        COLORREF borderCol = RGB(45, 58, 75);
        if (btn.isActive)
            borderCol = btn.accentColor;
        if (isHovered)
            borderCol = RGB(255, 255, 255);
        if (isPressed)
            borderCol = btn.accentColor;

        HPEN pen = CreatePen(PS_SOLID, (btn.isActive || isHovered) ? 2 : 1, borderCol);
        HGDIOBJ prevPen = SelectObject(dc, pen);
        HGDIOBJ prevBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

        Rectangle(dc, btn.rect.left, btn.rect.top, btn.rect.right, btn.rect.bottom);

        SelectObject(dc, prevBrush);
        SelectObject(dc, prevPen);
        DeleteObject(pen);

        // Draw Button Text
        if (btn.id == BTN_TAB_CONSOLE)
        {
            SelectObject(dc, fontNormal);
            SetTextColor(dc, isHovered ? RGB(255, 255, 255) : RGB(104, 216, 193));
            RECT rcText = btn.rect;
            DrawTextW(dc, btn.title.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        else if (btn.id >= BTN_MODE_WARMUP && btn.id <= BTN_MODE_RESTART)
        {
            // Mode button with Title + Subtitle
            SelectObject(dc, fontBold);
            SetTextColor(dc, btn.isActive ? btn.accentColor : (isHovered ? RGB(255, 255, 255) : RGB(225, 230, 238)));
            RECT rcT = btn.rect;
            rcT.top += 7;
            rcT.bottom = rcT.top + 18;
            DrawTextW(dc, btn.title.c_str(), -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SelectObject(dc, fontSmall);
            SetTextColor(dc, btn.isActive ? RGB(245, 245, 245) : RGB(160, 175, 190));
            RECT rcS = btn.rect;
            rcS.top = rcT.bottom + 2;
            rcS.bottom -= 4;
            DrawTextW(dc, btn.subtitle.c_str(), -1, &rcS, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RTLREADING);
        }
        else if (btn.id == BTN_TOGGLE_FF)
        {
            SelectObject(dc, fontBold);
            SetTextColor(dc, m_bFriendlyFire ? RGB(80, 240, 140) : RGB(255, 90, 90));
            RECT rcT = btn.rect;
            rcT.top += 4;
            rcT.bottom = rcT.top + 18;
            DrawTextW(dc, btn.title.c_str(), -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RTLREADING);

            SelectObject(dc, fontSmall);
            SetTextColor(dc, RGB(180, 190, 205));
            RECT rcS = btn.rect;
            rcS.top = rcT.bottom + 1;
            rcS.bottom -= 3;
            DrawTextW(dc, btn.subtitle.c_str(), -1, &rcS, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RTLREADING);
        }
        else if (btn.id == BTN_FREEZE_MINUS || btn.id == BTN_FREEZE_PLUS)
        {
            SelectObject(dc, fontTitle);
            SetTextColor(dc, isHovered ? RGB(255, 255, 255) : RGB(245, 201, 112));
            RECT rcT = btn.rect;
            DrawTextW(dc, btn.title.c_str(), -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        else
        {
            // Standard Chip (Freeze time / Round time / Money)
            SelectObject(dc, fontBold);
            SetTextColor(dc, btn.isActive ? btn.accentColor : (isHovered ? RGB(255, 255, 255) : RGB(220, 225, 235)));
            RECT rcT = btn.rect;
            if (!btn.subtitle.empty())
            {
                rcT.top += 3;
                rcT.bottom = rcT.top + 15;
                DrawTextW(dc, btn.title.c_str(), -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(dc, fontSmall);
                SetTextColor(dc, btn.isActive ? RGB(245, 245, 245) : RGB(145, 160, 175));
                RECT rcS = btn.rect;
                rcS.top = rcT.bottom;
                rcS.bottom -= 2;
                DrawTextW(dc, btn.subtitle.c_str(), -1, &rcS, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RTLREADING);
            }
            else
            {
                DrawTextW(dc, btn.title.c_str(), -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }

    // 5. Footer Status Bar
    RECT rcFooter{ 0, height - 26, width, height };
    SetDCBrushColor(dc, RGB(12, 16, 22));
    FillRect(dc, &rcFooter, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    SelectObject(dc, fontSmall);
    SetTextColor(dc, RGB(130, 150, 170));
    RECT rcStatusText{ 16, height - 24, width - 16, height - 2 };

    wchar_t statusBuf[256];
    swprintf_s(statusBuf,
        L"وضعیت هاست: حالت [%hs] | فریزتایم: %d ثانیه | تیر به خودی: [%s] | سرمایه: $%d | تغییرات آنی اعمال می‌شوند",
        m_currentMode.c_str(),
        m_iFreezeTime,
        m_bFriendlyFire ? L"روشن" : L"خاموش",
        m_iStartMoney
    );
    DrawTextW(dc, statusBuf, -1, &rcStatusText, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    GdiFlush();

    const auto* bgra = static_cast<const unsigned char*>(pixels);
    std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < rgba.size(); i += 4)
    {
        rgba[i] = bgra[i + 2];
        rgba[i + 1] = bgra[i + 1];
        rgba[i + 2] = bgra[i];
        rgba[i + 3] = 255;
    }

    SelectObject(dc, prevFont);
    SelectObject(dc, prevBmp);
    DeleteObject(fontSmall);
    DeleteObject(fontNormal);
    DeleteObject(fontBold);
    DeleteObject(fontTitle);
    DeleteObject(bitmap);
    DeleteDC(dc);

    return rgba;
}

void CLanHostMenuPanel::Paint()
{
    const int width = GetWide();
    const int height = GetTall();

    if (width <= 0 || height <= 0)
        return;

    if (!m_texture || width != m_width || height != m_height || m_needsRedraw)
    {
        const auto rgba = RenderHostMenuBitmap(width, height);
        if (rgba.empty())
            return;

        if (!m_texture)
            m_texture = vgui2::surface()->CreateNewTextureID(true);

        vgui2::surface()->DrawSetTextureRGBA(m_texture, rgba.data(), width, height, 0, true);
        m_width = width;
        m_height = height;
        m_needsRedraw = false;
    }

    vgui2::surface()->DrawSetColor(255, 255, 255, 255);
    vgui2::surface()->DrawSetTexture(m_texture);
    vgui2::surface()->DrawTexturedRect(0, 0, width, height);
}
