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

    // 1. Header Button: Switch to Console View (clean English)
    RECT rcConsoleTab{ width - 150, 8, width - 12, 34 };
    m_hitboxes.push_back({
        BTN_TAB_CONSOLE,
        rcConsoleTab,
        "",
        L"Console View",
        L"Switch to logs",
        false,
        false,
        RGB(104, 216, 193)
    });

    // 2. Section: Game Modes + Dedicated Separated Restart Button
    const int modesTop = 58;
    const int modesBottom = 114;
    const int leftPad = 12;
    const int rightPad = 12;
    const int availableW = width - leftPad - rightPad;
    const int gap = 8;

    // Restart button separated on the right:
    const int restartW = 145;
    const int modesAreaW = availableW - restartW - 12;
    const int numModes = 3;
    const int modeBtnW = (modesAreaW - (gap * (numModes - 1))) / numModes;

    struct ModeDef {
        ButtonId id;
        std::string modeKey;
        std::wstring title;
        std::wstring subtitle;
        COLORREF accent;
    };

    static const ModeDef modes[] = {
        { BTN_MODE_WARMUP,  "warm", L"WARMUP",    L"تمرینی و وارم‌آپ", RGB(245, 185, 60) },
        { BTN_MODE_MATCH,   "mix",  L"MATCH 5v5", L"مسابقه رسمی ۵ به ۵", RGB(60, 200, 245) },
        { BTN_MODE_1V1,     "1v1",  L"1v1 DUEL",  L"دوئل و ایم مپ",    RGB(190, 110, 255) }
    };

    int curX = leftPad;
    for (int i = 0; i < numModes; ++i)
    {
        RECT rcBtn{ curX, modesTop, curX + modeBtnW, modesBottom };
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
        curX += modeBtnW + gap;
    }

    // Dedicated Separated Restart Button (r / sv_restart 1)
    RECT rcRestart{ leftPad + modesAreaW + 12, modesTop, width - rightPad, modesBottom };
    m_hitboxes.push_back({
        BTN_RESTART,
        rcRestart,
        "r",
        L"🔄 RESTART",
        L"ری‌استارت راند (r)",
        false,
        false,
        RGB(50, 220, 130)
    });

    // 3. Section 2: Adaptive Settings Cards (Friendly Fire, Round Time, Freeze Time, Start Money)
    const int cardsTop = 138;
    const int footerH = 26;
    const int cardsBottom = height - footerH - 6;
    const int totalCardsH = std::max(cardsBottom - cardsTop, 190);
    const int cardGap = 7;
    const int cardH = std::clamp((totalCardsH - (2 * cardGap)) / 3, 58, 86);

    const int rowATop = cardsTop;
    const int rowABottom = rowATop + cardH;

    const int rowBTop = rowABottom + cardGap;
    const int rowBBottom = rowBTop + cardH;

    const int rowCTop = rowBBottom + cardGap;
    const int rowCBottom = rowCTop + cardH;

    const int btnH = std::clamp(cardH - 26, 28, 38);

    // Row A - Friendly Fire (Left half)
    const int halfW = (availableW - cardGap) / 2;
    RECT rcFF{ leftPad + 8, rowATop + 22, leftPad + halfW - 8, rowATop + 22 + btnH };
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

    // Row A - Round Time (Right half: 4 chips -> 1:00, 1:45 [1.75], 2:00, 5:00)
    const int rtAreaX = leftPad + halfW + cardGap + 8;
    const int rtAreaW = halfW - 16;
    const int rtChipW = (rtAreaW - (3 * 6)) / 4;
    const int rtChipY = rowATop + 22;

    struct RoundDef { ButtonId id; float val; const wchar_t* label; const wchar_t* sub; };
    static const RoundDef roundDefs[] = {
        { BTN_ROUND_100, 1.00f, L"1:00", L"1 دقیقه" },
        { BTN_ROUND_145, 1.75f, L"1:45", L"1.45 (رسمی)" },
        { BTN_ROUND_200, 2.00f, L"2:00", L"2 دقیقه" },
        { BTN_ROUND_500, 5.00f, L"5:00", L"5 دقیقه" }
    };

    for (int i = 0; i < 4; ++i)
    {
        RECT rcChip{
            rtAreaX + i * (rtChipW + 6),
            rtChipY,
            rtAreaX + i * (rtChipW + 6) + rtChipW,
            rtChipY + btnH
        };
        bool isActive = std::abs(m_fRoundTime - roundDefs[i].val) < 0.05f;
        m_hitboxes.push_back({
            roundDefs[i].id,
            rcChip,
            "",
            roundDefs[i].label,
            roundDefs[i].sub,
            false,
            isActive,
            RGB(60, 200, 245)
        });
    }

    // Row B - Freeze Time (Steppers [-] [+] and 4 Preset Chips: 0s, 5s, 8s, 12s)
    const int ftBtnY = rowBTop + 22;
    RECT rcMinus{ leftPad + 8, ftBtnY, leftPad + 46, ftBtnY + btnH };
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

    RECT rcPlus{ leftPad + 52, ftBtnY, leftPad + 90, ftBtnY + btnH };
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

    const int ftChipsStartX = leftPad + 98;
    const int ftChipsAvailableW = width - rightPad - ftChipsStartX - 8;
    const int ftChipW = (ftChipsAvailableW - (3 * 8)) / 4;

    struct FreezeDef { ButtonId id; int sec; const wchar_t* label; const wchar_t* sub; };
    static const FreezeDef freezeDefs[] = {
        { BTN_FREEZE_0,   0, L"0s",  L"بدون فریز" },
        { BTN_FREEZE_5,   5, L"5s",  L"کوتاه" },
        { BTN_FREEZE_8,   8, L"8s",  L"استاندارد" },
        { BTN_FREEZE_12, 12, L"12s", L"مسابقه رسمی" }
    };

    for (int i = 0; i < 4; ++i)
    {
        RECT rcFtChip{
            ftChipsStartX + i * (ftChipW + 8),
            ftBtnY,
            ftChipsStartX + i * (ftChipW + 8) + ftChipW,
            ftBtnY + btnH
        };
        bool isActive = (m_iFreezeTime == freezeDefs[i].sec);
        m_hitboxes.push_back({
            freezeDefs[i].id,
            rcFtChip,
            "",
            freezeDefs[i].label,
            freezeDefs[i].sub,
            false,
            isActive,
            RGB(80, 230, 140)
        });
    }

    // Row C - Start Money (3 Chips: $800, $5,000, $16,000)
    const int smBtnY = rowCTop + 22;
    const int smChipsStartX = leftPad + 8;
    const int smChipsAvailableW = width - rightPad - smChipsStartX - 8;
    const int smChipW = (smChipsAvailableW - (2 * 12)) / 3;

    struct MoneyDef { ButtonId id; int money; const wchar_t* title; const wchar_t* sub; };
    static const MoneyDef moneyDefs[] = {
        { BTN_MONEY_800,  800,   L"$800",    L"پیستول راند (Pistol)" },
        { BTN_MONEY_5000, 5000,  L"$5,000",  L"نیمه‌بای / تمرینی (Half)" },
        { BTN_MONEY_16K,  16000, L"$16,000", L"فول‌بای رسمی (Full Buy)" }
    };

    for (int i = 0; i < 3; ++i)
    {
        RECT rcSmChip{
            smChipsStartX + i * (smChipW + 12),
            smBtnY,
            smChipsStartX + i * (smChipW + 12) + smChipW,
            smBtnY + btnH
        };
        bool isActive = (m_iStartMoney == moneyDefs[i].money);
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

    case BTN_RESTART:
        if (engine)
        {
            engine->pfnClientCmd("r\nsv_restartround 1\n");
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
    case BTN_FREEZE_5:   SetFreezeTime(5); break;
    case BTN_FREEZE_8:   SetFreezeTime(8); break;
    case BTN_FREEZE_12:  SetFreezeTime(12); break;

    case BTN_ROUND_100: SetRoundTime(1.00f); break;
    case BTN_ROUND_145: SetRoundTime(1.75f); break;
    case BTN_ROUND_200: SetRoundTime(2.00f); break;
    case BTN_ROUND_500: SetRoundTime(5.00f); break;

    case BTN_MONEY_800:  SetStartMoney(800); break;
    case BTN_MONEY_5000: SetStartMoney(5000); break;
    case BTN_MONEY_16K:  SetStartMoney(16000); break;

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
        m_iStartMoney = 16000;
        if (m_pConsoleDialog)
            m_pConsoleDialog->PrintHostStatusCard("حالت تمرینی و گرم‌کردن فعال شد (WARMUP)", "تمرینی (WARMUP)", false, 0, 5.0f, 16000);
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
        m_fRoundTime = 1.0f;
        m_iStartMoney = 16000;
        if (m_pConsoleDialog)
            m_pConsoleDialog->PrintHostStatusCard("دوئل ۱ به ۱ فعال شد (1v1 DUEL)", "دوئل تک‌به‌تک (1v1 DUEL)", false, 0, 1.0f, 16000);
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
    const char* displayStr = (std::abs(minutes - 1.75f) < 0.05f) ? "1:45" : (minutes >= 4.9f ? "5:00" : (minutes >= 1.9f ? "2:00" : "1:00"));
    std::string cmd = std::format("mp_roundtime {:.2f}; wait; say === ROUND TIME {} MIN ===\n", minutes, displayStr);
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

    HFONT fontTitle = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HFONT fontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HFONT fontNormal = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HFONT fontSmall = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

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
    RECT rcTitleText{ 16, 4, width - 170, 24 };
    DrawTextW(dc, L"🎮  GAMELAND MATCH & HOST MANAGER", -1, &rcTitleText, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Header Subtitle (Persian RTL)
    SetTextColor(dc, RGB(104, 216, 193));
    SelectObject(dc, fontNormal);
    RECT rcSubText{ 16, 23, width - 170, 41 };
    DrawTextW(dc, L"کنترل‌پنل مدیریت مسابقه و هاست لن | اجرای سریع حالت‌ها و قوانین", -1, &rcSubText,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // 3. Section 1 Labels (Modes on left, Separated Quick Action on right)
    SelectObject(dc, fontBold);
    SetTextColor(dc, RGB(230, 235, 240));
    RECT rcSec1{ 16, 43, width - 175, 58 };
    DrawTextW(dc, L"حالت‌های بازی (Select Match Mode):", -1, &rcSec1,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    SelectObject(dc, fontSmall);
    SetTextColor(dc, RGB(80, 230, 140));
    RECT rcSec1R{ width - 165, 43, width - 16, 58 };
    DrawTextW(dc, L"عملیات ویژه (Quick Action):", -1, &rcSec1R,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Section 2 Labels
    SelectObject(dc, fontBold);
    SetTextColor(dc, RGB(230, 235, 240));
    RECT rcSec2{ 16, 120, width - 16, 136 };
    DrawTextW(dc, L"قوانین و تنظیمات لحظه‌ای سرور (Live Match Settings):", -1, &rcSec2,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Draw Card Backgrounds for Sections (matches BuildHitboxes adaptive layout)
    const int leftPad = 12;
    const int rightPad = 12;
    const int availableW = width - leftPad - rightPad;
    const int gap = 8;

    const int cardsTop = 138;
    const int footerH = 26;
    const int cardsBottom = height - footerH - 6;
    const int totalCardsH = std::max(cardsBottom - cardsTop, 190);
    const int cardGap = 7;
    const int cardH = std::clamp((totalCardsH - (2 * cardGap)) / 3, 58, 86);

    const int rowATop = cardsTop;
    const int rowABottom = rowATop + cardH;

    const int rowBTop = rowABottom + cardGap;
    const int rowBBottom = rowBTop + cardH;

    const int rowCTop = rowBBottom + cardGap;
    const int rowCBottom = rowCTop + cardH;

    const int halfW = (availableW - cardGap) / 2;

    // Card Friendly Fire
    RECT rcCardFF{ leftPad, rowATop, leftPad + halfW, rowABottom };
    SetDCBrushColor(dc, RGB(22, 28, 36));
    FillRect(dc, &rcCardFF, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    SelectObject(dc, fontSmall);
    SetTextColor(dc, RGB(160, 175, 190));
    RECT rcLblFF{ leftPad + 8, rowATop + 3, leftPad + halfW - 8, rowATop + 20 };
    DrawTextW(dc, L"وضعیت آسیب به هم‌تیمی‌ها (Friendly Fire):", -1, &rcLblFF,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Card Round Time
    RECT rcCardRT{ leftPad + halfW + cardGap, rowATop, width - rightPad, rowABottom };
    FillRect(dc, &rcCardRT, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    RECT rcLblRT{ leftPad + halfW + cardGap + 8, rowATop + 3, width - rightPad - 8, rowATop + 20 };
    wchar_t rtBuf[64];
    if (std::abs(m_fRoundTime - 1.75f) < 0.05f)
        swprintf_s(rtBuf, L"زمان هر راند: [1:45 دقیقه] (Round Time)");
    else
        swprintf_s(rtBuf, L"زمان هر راند: [%.0f:00 دقیقه] (Round Time)", m_fRoundTime);
    DrawTextW(dc, rtBuf, -1, &rcLblRT,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Card Freeze Time
    RECT rcCardFT{ leftPad, rowBTop, width - rightPad, rowBBottom };
    FillRect(dc, &rcCardFT, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    RECT rcLblFT{ leftPad + 8, rowBTop + 3, width - rightPad - 8, rowBTop + 20 };
    wchar_t ftBuf[64];
    swprintf_s(ftBuf, L"وقت اولیه ابتدای راند: [%d ثانیه] (Freeze Time)", m_iFreezeTime);
    DrawTextW(dc, ftBuf, -1, &rcLblFT,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Card Start Money
    RECT rcCardSM{ leftPad, rowCTop, width - rightPad, rowCBottom };
    FillRect(dc, &rcCardSM, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    RECT rcLblSM{ leftPad + 8, rowCTop + 3, width - rightPad - 8, rowCTop + 20 };
    wchar_t smBuf[64];
    swprintf_s(smBuf, L"سرمایه اولیه بازیکنان: [$%d] (Start Money)", m_iStartMoney);
    DrawTextW(dc, smBuf, -1, &rcLblSM,
        DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT | DT_RTLREADING);

    // Draw Subtle Outlines for All Cards
    HPEN cardPen = CreatePen(PS_SOLID, 1, RGB(36, 48, 64));
    HGDIOBJ prevCardPen = SelectObject(dc, cardPen);
    HGDIOBJ prevBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

    Rectangle(dc, rcCardFF.left, rcCardFF.top, rcCardFF.right, rcCardFF.bottom);
    Rectangle(dc, rcCardRT.left, rcCardRT.top, rcCardRT.right, rcCardRT.bottom);
    Rectangle(dc, rcCardFT.left, rcCardFT.top, rcCardFT.right, rcCardFT.bottom);
    Rectangle(dc, rcCardSM.left, rcCardSM.top, rcCardSM.right, rcCardSM.bottom);

    SelectObject(dc, prevBrush);
    SelectObject(dc, prevCardPen);
    DeleteObject(cardPen);

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
        else if (btn.id == BTN_RESTART)
        {
            SelectObject(dc, fontBold);
            SetTextColor(dc, isHovered ? RGB(255, 255, 255) : RGB(80, 240, 140));
            RECT rcT = btn.rect;
            rcT.top += 7;
            rcT.bottom = rcT.top + 18;
            DrawTextW(dc, btn.title.c_str(), -1, &rcT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SelectObject(dc, fontSmall);
            SetTextColor(dc, isHovered ? RGB(220, 255, 230) : RGB(140, 220, 170));
            RECT rcS = btn.rect;
            rcS.top = rcT.bottom + 2;
            rcS.bottom -= 4;
            DrawTextW(dc, btn.subtitle.c_str(), -1, &rcS, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RTLREADING);
        }
        else if (btn.id >= BTN_MODE_WARMUP && btn.id <= BTN_MODE_1V1)
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
    RECT rcFooter{ 0, height - footerH, width, height };
    SetDCBrushColor(dc, RGB(12, 16, 22));
    FillRect(dc, &rcFooter, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    SelectObject(dc, fontSmall);
    SetTextColor(dc, RGB(130, 150, 170));
    RECT rcStatusText{ 16, height - footerH + 2, width - 16, height - 2 };

    wchar_t rtDisplay[32];
    if (std::abs(m_fRoundTime - 1.75f) < 0.05f)
        wcscpy_s(rtDisplay, L"1:45");
    else
        swprintf_s(rtDisplay, L"%.0f:00", m_fRoundTime);

    wchar_t statusBuf[256];
    swprintf_s(statusBuf,
        L"وضعیت هاست: حالت [%hs] | زمان راند: %s | فریزتایم: %d ثانیه | تیر به خودی: [%s] | سرمایه: $%d | اعمال خودکار",
        m_currentMode.c_str(),
        rtDisplay,
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
