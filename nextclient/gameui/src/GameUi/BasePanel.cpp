#include "GameUi.h"
#include "GameUINext.h"
#include "BasePanel.h"

#include "vgui/IInput.h"
#include "vgui/IInputInternal.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/ISurfaceNext.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "FileSystem.h"
#include <tier2/tier2.h>

#include "IGameUIFuncs.h"

#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ControllerMap.h"
#include "vgui_controls/KeyRepeat.h"

#include "ModInfo.h"
#include "LoadingDialog.h"
#include "BackgroundMenuButton.h"
#include "CreateMultiplayerGameDialog.h"

#include "ToolBar.h"
#include "GameConsole.h"
#include "PlayerListDialog.h"
#include "OnlinePlayersDialog.h"
#include "LobbyChatDialog.h"
#include "PresenceClient.h"
#include "PersianShaper.h"
#include "../ServerBrowser/ServerBrowserDialog.h"

#include <keydefs.h>

#include <nitro_utils/string_utils.h>

#include <algorithm>
#include <vector>
#include <format>

// undef windows stuff
#undef PostMessage

static CBasePanel *g_pBasePanel = NULL;
static float g_flAnimationPadding = 0.01f;

CBasePanel *BasePanel(void)
{
    return g_pBasePanel;
}

vgui2::VPANEL GetGameUIBasePanel(void)
{
    return BasePanel()->GetVPanel();
}

CGameMenuItem::CGameMenuItem(vgui2::Menu *parent, const char *name) : BaseClass(parent, name, "GameMenuItem")
{
    m_bRightAligned = false;
    m_bIsDemoStudio = false;
    m_bIsQuit = false;
    m_bIsResume = false;
    m_bIsDisconnect = false;
    m_flScale = 1.0f;
}

void CGameMenuItem::UpdateScaleMetrics(float scale)
{
    m_flScale = scale;
    int baseInset = (int)(24.0f * m_flScale + 0.5f);
    SetTextInset(baseInset, 0);
}

void CGameMenuItem::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    int swide, stall;
    vgui2::surface()->GetScreenSize(swide, stall);
    float scale = (float)stall / 600.0f;
    if (scale < 0.75f) scale = 0.75f;
    m_flScale = scale;

    m_bIsDemoStudio = false;
    m_bIsQuit = false;
    m_bIsResume = false;
    m_bIsDisconnect = false;

    if (GetCommand())
    {
        const char *cmd = GetCommand()->GetString("command", "");
        if (!Q_stricmp(cmd, "OpenDemoStudio") || !Q_stricmp(cmd, "OpenDemoUploader"))
            m_bIsDemoStudio = true;
        else if (!Q_stricmp(cmd, "Quit"))
            m_bIsQuit = true;
        else if (!Q_stricmp(cmd, "ResumeGame"))
            m_bIsResume = true;
        else if (!Q_stricmp(cmd, "Disconnect"))
            m_bIsDisconnect = true;
    }

    Color fgColor = Color(225, 230, 240, 240);
    Color armedColor = Color(255, 255, 255, 255);
    Color depressedColor = Color(200, 205, 215, 255);

    if (m_bIsDemoStudio)
    {
        fgColor = Color(0, 230, 255, 255);       // Electric Cyan
        armedColor = Color(255, 255, 255, 255);
    }
    else if (m_bIsQuit || m_bIsDisconnect)
    {
        fgColor = Color(225, 225, 230, 220);
        armedColor = Color(255, 110, 110, 255);   // Warning Rose / Red
    }

    SetFgColor(fgColor);
    SetBgColor(Color(0, 0, 0, 0));
    SetDefaultColor(fgColor, Color(0, 0, 0, 0));
    SetArmedColor(armedColor, Color(0, 0, 0, 0));
    SetDepressedColor(depressedColor, Color(0, 0, 0, 0));
    SetContentAlignment(Label::a_west);

    SetBorder(NULL);
    SetDefaultBorder(NULL);
    SetDepressedBorder(NULL);
    SetKeyFocusBorder(NULL);

    vgui2::HFont hMenuFont = pScheme->GetFont("MenuLarge", IsProportional());
    if (!hMenuFont)
        hMenuFont = pScheme->GetFont("DefaultBold", IsProportional());
    if (hMenuFont)
        SetFont(hMenuFont);

    int baseInset = (int)(24.0f * m_flScale + 0.5f);
    SetTextInset(baseInset, 0);
    SetArmedSound("UI/buttonrollover.wav");
    SetDepressedSound("UI/buttonclick.wav");
    SetReleasedSound("UI/buttonclickrelease.wav");
    SetButtonActivationType(Button::ACTIVATE_ONPRESSED);
    SetPaintBackgroundType(0);

    if (m_bRightAligned)
        SetContentAlignment(Label::a_east);
}

void CGameMenuItem::PaintBackground(void)
{
    int w, h;
    GetSize(w, h);

    bool isArmed = IsArmed();
    bool isDepressed = IsDepressed();

    // Subtle 1px vertical spacing between cards
    int cardY0 = 1;
    int cardY1 = h - 1;

    int accentW = std::max(3, (int)(4.0f * m_flScale + 0.5f));
    int innerGlowW = accentW + std::max(2, (int)(2.0f * m_flScale + 0.5f));
    int dotSize = std::max(4, (int)(4.0f * m_flScale + 0.5f));
    int dotLeft = (int)(11.0f * m_flScale + 0.5f);
    int dotRight = dotLeft + dotSize;
    int dotTop = h / 2 - dotSize / 2;
    int dotBottom = dotTop + dotSize;

    if (m_bIsDemoStudio)
    {
        // ─── Match Demo Studio (F4): Neon Cyan Cyber Card ───
        if (isArmed)
        {
            // Dark cyan frosted glass background
            vgui2::surface()->DrawSetColor(Color(12, 42, 64, 235));
            vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);

            // Left luminous neon cyan accent bar
            vgui2::surface()->DrawSetColor(Color(0, 245, 255, 255));
            vgui2::surface()->DrawFilledRect(0, cardY0, accentW, cardY1);

            // Inner cyan accent glow
            vgui2::surface()->DrawSetColor(Color(0, 210, 255, 120));
            vgui2::surface()->DrawFilledRect(accentW, cardY0, innerGlowW, cardY1);

            // Glowing cyan card border
            vgui2::surface()->DrawSetColor(Color(0, 245, 255, 140));
            vgui2::surface()->DrawOutlinedRect(0, cardY0, w, cardY1);

            // Bright indicator dot
            vgui2::surface()->DrawSetColor(Color(0, 255, 255, 255));
            vgui2::surface()->DrawFilledRect(dotLeft, dotTop, dotRight, dotBottom);
        }
        else
        {
            // Idle: refined translucent cyan pill
            vgui2::surface()->DrawSetColor(Color(10, 26, 42, 165));
            vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);

            // Cyan left accent edge
            vgui2::surface()->DrawSetColor(Color(0, 210, 255, 180));
            vgui2::surface()->DrawFilledRect(0, cardY0, std::max(2, accentW - 1), cardY1);

            // Hairline border
            vgui2::surface()->DrawSetColor(Color(0, 210, 255, 50));
            vgui2::surface()->DrawOutlinedRect(0, cardY0, w, cardY1);

            // Cyan indicator dot
            vgui2::surface()->DrawSetColor(Color(0, 210, 255, 190));
            vgui2::surface()->DrawFilledRect(dotLeft, dotTop, dotRight, dotBottom);
        }
    }
    else if (m_bIsQuit || m_bIsDisconnect)
    {
        // ─── Quit / Disconnect: Sleek Crimson / Dark Slate Card ───
        if (isArmed)
        {
            vgui2::surface()->DrawSetColor(Color(52, 18, 24, 230));
            vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 70, 70, 255));
            vgui2::surface()->DrawFilledRect(0, cardY0, accentW, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 75, 75, 110));
            vgui2::surface()->DrawOutlinedRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 80, 80, 255));
            vgui2::surface()->DrawFilledRect(dotLeft, dotTop, dotRight, dotBottom);
        }
        else
        {
            vgui2::surface()->DrawSetColor(Color(16, 20, 26, 125));
            vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 255, 255, 14));
            vgui2::surface()->DrawOutlinedRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(160, 165, 175, 130));
            vgui2::surface()->DrawFilledRect(dotLeft, dotTop, dotRight, dotBottom);
        }
    }
    else
    {
        // ─── Standard Menu Items: Modern Glass / Amber Card ───
        if (isArmed)
        {
            vgui2::surface()->DrawSetColor(Color(34, 40, 52, 235));
            vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 185, 45, 255));
            vgui2::surface()->DrawFilledRect(0, cardY0, accentW, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 195, 60, 100));
            vgui2::surface()->DrawFilledRect(accentW, cardY0, innerGlowW, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 205, 80, 100));
            vgui2::surface()->DrawOutlinedRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 200, 55, 255));
            vgui2::surface()->DrawFilledRect(dotLeft, dotTop, dotRight, dotBottom);
        }
        else
        {
            vgui2::surface()->DrawSetColor(Color(18, 22, 30, 140));
            vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(255, 255, 255, 16));
            vgui2::surface()->DrawOutlinedRect(0, cardY0, w, cardY1);

            vgui2::surface()->DrawSetColor(Color(150, 160, 175, 130));
            vgui2::surface()->DrawFilledRect(dotLeft, dotTop, dotRight, dotBottom);
        }
    }

    if (isDepressed)
    {
        vgui2::surface()->DrawSetColor(Color(255, 255, 255, 50));
        vgui2::surface()->DrawFilledRect(0, cardY0, w, cardY1);
    }
}

void CGameMenuItem::SetRightAlignedText(bool state)
{
    m_bRightAligned = state;
}

Color CGameMenuItem::GetButtonBgColor()
{
    return GetBgColor();
}

Color CGameMenuItem::GetButtonFgColor()
{
    return BaseClass::GetButtonFgColor();
}

void CGameMenuItem::OnCursorEntered(void)
{
    BaseClass::OnCursorEntered();
    int slideInset = (int)(30.0f * m_flScale + 0.5f);
    SetTextInset(slideInset, 0); // Smooth responsive slide to right
    Repaint();
}

void CGameMenuItem::OnCursorExited(void)
{
    BaseClass::OnCursorExited();
    int baseInset = (int)(24.0f * m_flScale + 0.5f);
    SetTextInset(baseInset, 0); // Smooth slide back
    Repaint();
}

class CGameMenu : public vgui2::Menu
{
    DECLARE_CLASS_SIMPLE(CGameMenu, vgui2::Menu);

public:
    CGameMenu(vgui2::Panel *parent, const char *name) : BaseClass(parent, name)
    {
        m_pConsoleFooter = NULL;
    }

    virtual void ApplySchemeSettings(vgui2::IScheme *pScheme)
    {
        BaseClass::ApplySchemeSettings(pScheme);

        int swide, stall;
        vgui2::surface()->GetScreenSize(swide, stall);
        float scale = (float)stall / 600.0f;
        if (scale < 0.75f) scale = 0.75f;

        int itemH = (int)(34.0f * scale + 0.5f);
        SetMenuItemHeight(itemH);

        SetBgColor(Color(0, 0, 0, 0));
        SetBorder(NULL);
        SetPaintBackgroundEnabled(false);
    }

    virtual void LayoutMenuBorder(void)
    {
    }

    virtual void SetVisible(bool state)
    {
        BaseClass::SetVisible(true);

        if (!state)
            vgui2::ipanel()->MoveToBack(GetVPanel());
    }

    virtual int AddMenuItem(const char *itemName, const char *itemText, const char *command, Panel *target, KeyValues *userData = NULL)
    {
        vgui2::MenuItem *item = new CGameMenuItem(this, itemName);
        item->AddActionSignalTarget(target);
        item->SetCommand(command);
        item->SetText(itemText);
        item->SetUserData(userData);

        return BaseClass::AddMenuItem(item);
    }

    virtual void SetMenuItemBlinkingState(const char *itemName, bool state)
    {
        for (int i = 0; i < GetChildCount(); i++)
        {
            Panel *child = GetChild(i);
            vgui2::MenuItem *menuItem = dynamic_cast<vgui2::MenuItem *>(child);

            if (menuItem)
            {
                if (Q_strcmp(menuItem->GetCommand()->GetString("command", ""), itemName) == 0)
                    menuItem->SetBlink(state);
            }
        }

        InvalidateLayout();
    }

    virtual void OnCommand(const char *command)
    {
        m_KeyRepeat.Reset();

        if (!stricmp(command, "Open"))
        {
            MoveToFront();
            RequestFocus();
        }
        else
            BaseClass::OnCommand(command);
    }

    virtual void PerformLayout(void)
    {
        BaseClass::PerformLayout();

        int swide = 0, stall = 0;
        vgui2::surface()->GetScreenSize(swide, stall);
        float scale = (stall > 0) ? ((float)stall / 600.0f) : 1.0f;
        if (scale < 1.0f) scale = 1.0f;

        int itemH = (int)(34.0f * scale + 0.5f);
        SetMenuItemHeight(itemH);

        // Uniform modern card width, scaled to 800x600 experience
        int maxItemW = (int)(220.0f * scale + 0.5f);
        for (int i = 0; i < GetChildCount(); i++)
        {
            Panel *child = GetChild(i);
            CGameMenuItem *menuItem = dynamic_cast<CGameMenuItem *>(child);
            if (menuItem && menuItem->IsVisible())
            {
                menuItem->UpdateScaleMetrics(scale);

                int mw = 0, mh = 0;
                menuItem->GetSize(mw, mh);
                if (mw > maxItemW)
                    maxItemW = mw;
            }
        }

        for (int i = 0; i < GetChildCount(); i++)
        {
            Panel *child = GetChild(i);
            CGameMenuItem *menuItem = dynamic_cast<CGameMenuItem *>(child);
            if (menuItem && menuItem->IsVisible())
            {
                menuItem->SetSize(maxItemW, itemH);
            }
        }

        bool foundLobbyChat = false;
        bool foundDemoStudio = false;
        int gapChat = (int)(10.0f * scale + 0.5f);
        int gapDemo = (int)(12.0f * scale + 0.5f);

        for (int i = 0; i < GetChildCount(); i++)
        {
            Panel *child = GetChild(i);
            CGameMenuItem *menuItem = dynamic_cast<CGameMenuItem *>(child);
            if (menuItem && menuItem->IsVisible())
            {
                const char *cmd = menuItem->GetCommand() ? menuItem->GetCommand()->GetString("command", "") : "";
                if (!Q_stricmp(cmd, "OpenLobbyChat") || !Q_stricmp(cmd, "OpenGlobalChat"))
                {
                    foundLobbyChat = true;
                    continue;
                }
                if (!Q_stricmp(cmd, "OpenDemoStudio") || !Q_stricmp(cmd, "OpenDemoUploader"))
                {
                    foundDemoStudio = true;
                    if (foundLobbyChat)
                    {
                        int x, y;
                        menuItem->GetPos(x, y);
                        menuItem->SetPos(x, y + gapChat);
                    }
                    continue;
                }

                int totalShift = 0;
                if (foundLobbyChat) totalShift += gapChat;
                if (foundDemoStudio) totalShift += gapDemo;

                if (totalShift > 0)
                {
                    int x, y;
                    menuItem->GetPos(x, y);
                    menuItem->SetPos(x, y + totalShift);
                }
            }
        }

        int w, h;
        GetSize(w, h);
        int totalExtraH = (foundLobbyChat ? gapChat : 0) + (foundDemoStudio ? gapDemo : 0);
        SetSize(std::max(w, maxItemW), h + totalExtraH);
    }

    virtual void OnKeyCodePressed(vgui2::KeyCode code)
    {
        if (code == vgui2::KEY_F3)
        {
            BasePanel()->OnOpenLobbyChatDialog();
            return;
        }
        if (code == vgui2::KEY_F4)
        {
            BasePanel()->OnOpenDemoUploaderDialog();
            return;
        }
        m_KeyRepeat.KeyDown(code);
        BaseClass::OnKeyCodePressed(code);
    }

    void OnKeyCodeReleased(vgui2::KeyCode code)
    {
        m_KeyRepeat.KeyUp(code);
        BaseClass::OnKeyCodeReleased(code);
    }

    void OnThink(void)
    {
        vgui2::KeyCode code = m_KeyRepeat.KeyRepeated();

        if (code)
            OnKeyCodeTyped(code);

        BaseClass::OnThink();
    }

    virtual void OnKillFocus(void)
    {
        BaseClass::OnKillFocus();

        vgui2::surface()->MovePopupToBack(GetVPanel());

        m_KeyRepeat.Reset();
    }

    void ShowFooter(bool bShow)
    {
        if (m_pConsoleFooter)
            m_pConsoleFooter->SetVisible(bShow);
    }

    void UpdateMenuItemState(bool isInGame, bool isMultiplayer)
    {
        for (int i = 0; i < GetChildCount(); i++)
        {
            Panel *child = GetChild(i);
            vgui2::MenuItem *menuItem = dynamic_cast<vgui2::MenuItem *>(child);

            if (menuItem)
            {
                bool shouldBeVisible = true;
                KeyValues *kv = menuItem->GetUserData();

                if (!kv)
                    continue;

                if (!isInGame && kv->GetInt("OnlyInGame"))
                    shouldBeVisible = false;
                else if (isMultiplayer && kv->GetInt("notmulti"))
                    shouldBeVisible = false;
                else if (isInGame && !isMultiplayer && kv->GetInt("notsingle"))
                    shouldBeVisible = false;
                else if (kv->GetInt("ConsoleOnly"))
                    shouldBeVisible = false;

                menuItem->SetVisible(shouldBeVisible);
            }
        }

        if (!isInGame)
        {
            for (int j = 0; j < GetChildCount() - 2; j++)
                MoveMenuItem(j, j + 1);
        }
        else
        {
            for (int i = 0; i < GetChildCount(); i++)
            {
                for (int j = i; j < GetChildCount() - 2; j++)
                {
                    int iID1 = GetMenuID(j);
                    int iID2 = GetMenuID(j + 1);

                    vgui2::MenuItem *menuItem1 = GetMenuItem(iID1);
                    vgui2::MenuItem *menuItem2 = GetMenuItem(iID2);

                    KeyValues *kv1 = menuItem1->GetUserData();
                    KeyValues *kv2 = menuItem2->GetUserData();
                    if (kv1 && kv2)
                    {
                        if (kv1->GetInt("InGameOrder") > kv2->GetInt("InGameOrder"))
                            MoveMenuItem(iID2, iID1);
                    }
                }
            }
        }

        InvalidateLayout();

        if (m_pConsoleFooter)
        {
            const char *pHelpName;

            if (!isInGame)
                pHelpName = "MainMenu";
            else
                pHelpName = "GameMenu";

            if (!m_pConsoleFooter->GetHelpName() || V_stricmp(pHelpName, m_pConsoleFooter->GetHelpName()))
            {
                m_pConsoleFooter->SetHelpNameAndReset(pHelpName);
                m_pConsoleFooter->AddNewButtonLabel("#GameUI_Action", "#GameUI_Icons_A_BUTTON");

                if (isInGame)
                    m_pConsoleFooter->AddNewButtonLabel("#GameUI_Close", "#GameUI_Icons_B_BUTTON");
            }
        }
    }

private:
    CFooterPanel *m_pConsoleFooter;
    vgui2::CKeyRepeatHandler m_KeyRepeat;
};

static CBackgroundMenuButton *CreateMenuButton(CBasePanel *parent, const char *panelName, const wchar_t *panelText)
{
    CBackgroundMenuButton *pButton = new CBackgroundMenuButton(parent, panelName);
    pButton->SetProportional(true);
    pButton->SetCommand("OpenGameMenu");
    pButton->SetText(panelText);

    return pButton;
}

CBasePanel::CBasePanel(void) : vgui2::Panel(NULL, "BaseGameUIPanel")
{
    g_pBasePanel = this;
    m_bLevelLoading = false;
    m_eBackgroundState = BACKGROUND_INITIAL;
    m_flTransitionStartTime = 0.0f;
    m_flTransitionEndTime = 0.0f;
    m_flFrameFadeInTime = 0.5f;
    m_bRenderingBackgroundTransition = false;
    m_bFadingInMenus = false;
    m_bEverActivated = false;
    m_iGameMenuInset = 24;
    m_bHaveDarkenedBackground = false;
    m_bHaveDarkenedTitleText = true;
    m_bForceTitleTextUpdate = true;
    m_BackdropColor = Color(0, 0, 0, 128);
    m_pConsoleAnimationController = NULL;
    m_pConsoleControlSettings = NULL;
    m_iToolBarSize = 40;
    m_bInitialLoading = true;

    m_pGameMenu = NULL;
    m_pGameLogo = NULL;

    CreateGameMenu();
    // CreateGameLogo();
    CreateBackGround();
    CreateToolbar();
    SetMenuAlpha(0);

    m_pFocusParent = NULL;
    m_pFocusPanel = NULL;
}

KeyValues *CBasePanel::GetConsoleControlSettings(void)
{
    return m_pConsoleControlSettings;
}

CBasePanel::~CBasePanel(void)
{
    g_pBasePanel = NULL;
}

void CBasePanel::PaintBackground(void)
{
    if (!m_hOptionsDialog.Get())
    {
        m_hOptionsDialog = new COptionsDialog(this);
#if !defined(GAMELAND_HOME_CLIENT) || !GAMELAND_HOME_CLIENT
        m_hCreateMultiplayerGameDialog = new CCreateMultiplayerGameDialog(this);
#endif

        PositionDialog(m_hOptionsDialog);
#if !defined(GAMELAND_HOME_CLIENT) || !GAMELAND_HOME_CLIENT
        PositionDialog(m_hCreateMultiplayerGameDialog);
#endif
    }

    if (!GameUI().IsInLevel() || g_hLoadingDialog.Get())
    {
        DrawBackgroundImage();
    }

    if (m_flBackgroundFillAlpha)
    {
        int swide, stall;
        vgui2::surface()->GetScreenSize(swide, stall);
        vgui2::surface()->DrawSetColor(0, 0, 0, m_flBackgroundFillAlpha);
        vgui2::surface()->DrawFilledRect(0, 0, swide, stall);
    }

    DrawTopWelcomeBanner();
    DrawLeftClockWidget();
    DrawRightChatPreview();
}

void CBasePanel::DrawTopWelcomeBanner(void)
{
    if (GameUI().IsInLevel() || g_hLoadingDialog.Get())
        return;

    int swide = 0, stall = 0;
    vgui2::surface()->GetScreenSize(swide, stall);
    if (swide <= 0 || stall <= 0)
        return;

    float scale = (float)stall / 600.0f;
    if (scale < 1.0f) scale = 1.0f;

    // Get current player name from engine
    const char *pName = engine ? engine->pfnGetCvarString("name") : "Player";
    if (!pName || !*pName)
        pName = "Player";

    wchar_t wName[64]{};
    if (g_pVGuiLocalize)
        g_pVGuiLocalize->ConvertANSIToUnicode(pName, wName, sizeof(wName));
    else
        mbstowcs(wName, pName, sizeof(wName) / sizeof(wchar_t) - 1);

    wchar_t wPrefix[] = L"WELCOME : ";

    vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetDefaultScheme());
    if (!pScheme)
        return;

    vgui2::HFont hFont = pScheme->GetFont("MenuLarge", IsProportional());
    if (!hFont)
        hFont = pScheme->GetFont("DefaultBold", IsProportional());
    if (!hFont)
        return;

    int prefixW = 0, prefixH = 0;
    vgui2::surface()->GetTextSize(hFont, wPrefix, prefixW, prefixH);

    int nameW = 0, nameH = 0;
    vgui2::surface()->GetTextSize(hFont, wName, nameW, nameH);

    int totalOnline = PresenceClient::GetInstance().GetTotalOnline();
    int inLobby = PresenceClient::GetInstance().GetInLobby();
    int inGame = PresenceClient::GetInstance().GetInGame();
    bool hasData = PresenceClient::GetInstance().HasData();

    wchar_t wPresence[128]{};
    if (hasData)
    {
        swprintf_s(wPresence, L"  |  ONLINE: %d  (LOBBY: %d | MATCH: %d)", totalOnline, inLobby, inGame);
    }
    else
    {
        swprintf_s(wPresence, L"  |  CONNECTING...");
    }

    int presenceW = 0, presenceH = 0;
    vgui2::surface()->GetTextSize(hFont, wPresence, presenceW, presenceH);

    int textW = prefixW + nameW + presenceW;
    int textH = std::max({prefixH, nameH, presenceH});

    int paddingX = (int)(24.0f * scale + 0.5f);
    int dotSize = std::max(6, (int)(6.0f * scale + 0.5f));
    int dotGap = (int)(14.0f * scale + 0.5f);

    int bannerH = std::max((int)(36.0f * scale + 0.5f), textH + (int)(14.0f * scale + 0.5f));
    int bannerW = textW + paddingX * 2 + dotSize + dotGap;
    int bannerX = (swide - bannerW) / 2;
    int bannerY = (int)(20.0f * scale + 0.5f);

    m_presenceBannerX = bannerX;
    m_presenceBannerY = bannerY;
    m_presenceBannerW = bannerW;
    m_presenceBannerH = bannerH;

    // 1. Frosted obsidian glass backdrop
    vgui2::surface()->DrawSetColor(Color(14, 18, 26, 215));
    vgui2::surface()->DrawFilledRect(bannerX, bannerY, bannerX + bannerW, bannerY + bannerH);

    // 2. Subtle top accent line (Dual-tone cyan to gold)
    int halfW = bannerW / 2;
    int accentH = std::max(2, (int)(2.0f * scale + 0.5f));
    vgui2::surface()->DrawSetColor(Color(0, 220, 255, 230)); // Cyan
    vgui2::surface()->DrawFilledRect(bannerX, bannerY, bannerX + halfW, bannerY + accentH);
    vgui2::surface()->DrawSetColor(Color(255, 185, 45, 230)); // Esports Gold
    vgui2::surface()->DrawFilledRect(bannerX + halfW, bannerY, bannerX + bannerW, bannerY + accentH);

    // 3. Hairline outline
    vgui2::surface()->DrawSetColor(Color(255, 255, 255, 22));
    vgui2::surface()->DrawOutlinedRect(bannerX, bannerY, bannerX + bannerW, bannerY + bannerH);

    // 4. Live status green indicator dot
    int dotX = bannerX + paddingX;
    int dotY = bannerY + (bannerH - dotSize) / 2;
    vgui2::surface()->DrawSetColor(Color(46, 213, 115, 255));
    vgui2::surface()->DrawFilledRect(dotX, dotY, dotX + dotSize, dotY + dotSize);

    // 5. Draw text
    int textX = dotX + dotSize + dotGap;
    int textY = bannerY + (bannerH - textH) / 2;

    vgui2::surface()->DrawSetTextFont(hFont);

    // Draw "WELCOME : " in metallic silver/slate
    vgui2::surface()->DrawSetTextColor(Color(175, 185, 200, 255));
    vgui2::surface()->DrawSetTextPos(textX, textY);
    vgui2::surface()->DrawPrintText(wPrefix, wcslen(wPrefix));

    // Draw Player Name in luminous esports gold
    vgui2::surface()->DrawSetTextColor(Color(255, 215, 85, 255));
    vgui2::surface()->DrawSetTextPos(textX + prefixW, textY);
    vgui2::surface()->DrawPrintText(wName, wcslen(wName));

    // Draw Online Presence stats in vibrant emerald mint
    vgui2::surface()->DrawSetTextColor(Color(0, 220, 180, 255));
    vgui2::surface()->DrawSetTextPos(textX + prefixW + nameW, textY);
    vgui2::surface()->DrawPrintText(wPresence, wcslen(wPresence));
}

void CBasePanel::DrawRightChatPreview(void)
{
    if (GameUI().IsInLevel() || g_hLoadingDialog.Get())
        return;

    int swide = 0, stall = 0;
    vgui2::surface()->GetScreenSize(swide, stall);
    if (swide <= 0 || stall <= 0)
        return;

    float scale = (float)stall / 600.0f;
    if (scale < 1.0f) scale = 1.0f;

    vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetDefaultScheme());
    if (!pScheme)
        return;

    vgui2::HFont hFont = pScheme->GetFont("Default", IsProportional());
    vgui2::HFont hFontBold = pScheme->GetFont("DefaultBold", IsProportional());
    if (!hFont) hFont = hFontBold;
    if (!hFont) return;

    LobbyChatMessage latestMsg;
    bool hasMsg = PresenceClient::GetInstance().GetLatestChatMessage(latestMsg);
    time_t curUnix = time(nullptr);
    if (hasMsg && latestMsg.timestamp > 0 && (curUnix - latestMsg.timestamp) > 600)
    {
        hasMsg = false; // Expired after 10 minutes
    }

    int cardW = (int)(280.0f * scale + 0.5f);
    int cardH = (int)(80.0f * scale + 0.5f);
    int marginX = (int)(24.0f * scale + 0.5f);
    int cardX = swide - cardW - marginX;
    int cardY = (int)(75.0f * scale + 0.5f);

    m_chatPreviewX = cardX;
    m_chatPreviewY = cardY;
    m_chatPreviewW = cardW;
    m_chatPreviewH = cardH;

    // 1. Sleek translucent glass background (نامرئی‌طور و مدرن)
    vgui2::surface()->DrawSetColor(Color(12, 16, 24, 175));
    vgui2::surface()->DrawFilledRect(cardX, cardY, cardX + cardW, cardY + cardH);

    // 2. Cyan glowing outer border
    vgui2::surface()->DrawSetColor(Color(0, 205, 255, 80));
    vgui2::surface()->DrawOutlinedRect(cardX, cardY, cardX + cardW, cardY + cardH);

    // 3. Top accent highlight
    vgui2::surface()->DrawSetColor(Color(0, 230, 255, 160));
    vgui2::surface()->DrawFilledRect(cardX, cardY, cardX + cardW, cardY + 2);

    int padX = (int)(12.0f * scale + 0.5f);
    int curY = cardY + (int)(8.0f * scale + 0.5f);

    // 4. Header title: "LOBBY CHAT" + time
    wchar_t wHeader[64]{};
    if (hasMsg && !latestMsg.time.empty())
    {
        std::wstring wTime = Persian::Utf8ToWide(latestMsg.time);
        swprintf_s(wHeader, L"GLOBAL LOBBY CHAT  [%s]", wTime.c_str());
    }
    else
    {
        swprintf_s(wHeader, L"GLOBAL LOBBY CHAT");
    }

    vgui2::surface()->DrawSetTextFont(hFontBold ? hFontBold : hFont);
    vgui2::surface()->DrawSetTextColor(Color(0, 220, 255, 240));
    vgui2::surface()->DrawSetTextPos(cardX + padX, curY);
    vgui2::surface()->DrawPrintText(wHeader, wcslen(wHeader));

    curY += (int)(18.0f * scale + 0.5f);

    // 5. Message body
    vgui2::surface()->DrawSetTextFont(hFont);
    if (hasMsg && !latestMsg.text.empty())
    {
        std::string senderRaw = latestMsg.tag.empty() ? latestMsg.sender : ("[" + latestMsg.tag + "] " + latestMsg.sender);
        std::wstring wSender = Persian::ShapeAndBiDi(Persian::Utf8ToWide(senderRaw)) + L": ";

        vgui2::surface()->DrawSetTextColor(Color(255, 215, 85, 255));
        vgui2::surface()->DrawSetTextPos(cardX + padX, curY);
        vgui2::surface()->DrawPrintText(wSender.c_str(), wSender.length());

        int senderW = 0, senderH = 0;
        vgui2::surface()->GetTextSize(hFont, wSender.c_str(), senderW, senderH);

        std::wstring wTextRaw = Persian::Utf8ToWide(latestMsg.text);
        if (wTextRaw.length() > 28)
        {
            wTextRaw = wTextRaw.substr(0, 26) + L"...";
        }
        std::wstring wText = Persian::ShapeAndBiDi(wTextRaw);

        vgui2::surface()->DrawSetTextColor(Color(240, 245, 255, 255));
        vgui2::surface()->DrawSetTextPos(cardX + padX + senderW, curY);
        vgui2::surface()->DrawPrintText(wText.c_str(), wText.length());
    }
    else
    {
        wchar_t wNoMsg[] = L"No recent messages. Click to chat!";
        vgui2::surface()->DrawSetTextColor(Color(150, 165, 180, 210));
        vgui2::surface()->DrawSetTextPos(cardX + padX, curY);
        vgui2::surface()->DrawPrintText(wNoMsg, wcslen(wNoMsg));
    }

    curY += (int)(22.0f * scale + 0.5f);

    // 6. Subtext call to action
    wchar_t wFooter[] = L"Click or press F3 to join chat";
    vgui2::surface()->DrawSetTextColor(Color(115, 135, 155, 190));
    vgui2::surface()->DrawSetTextPos(cardX + padX, curY);
    vgui2::surface()->DrawPrintText(wFooter, wcslen(wFooter));
}

static void ConvertGregorianToJalali(int g_y, int g_m, int g_d, int& j_y, int& j_m, int& j_d)
{
    static const int g_days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    static const int j_days_in_month[] = { 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29 };

    int gy = g_y - 1600;
    int gm = g_m - 1;
    int gd = g_d - 1;

    int g_day_no = 365 * gy + (gy + 3) / 4 - (gy + 99) / 100 + (gy + 399) / 400;
    for (int i = 0; i < gm; ++i)
        g_day_no += g_days_in_month[i];
    if (gm > 1 && ((gy % 4 == 0 && gy % 100 != 0) || (gy % 400 == 0)))
        g_day_no++;
    g_day_no += gd;

    int j_day_no = g_day_no - 79;
    int j_np = j_day_no / 12053;
    j_day_no %= 12053;

    int jy = 979 + 33 * j_np + 4 * (j_day_no / 1461);
    j_day_no %= 1461;

    if (j_day_no >= 366) {
        jy += (j_day_no - 1) / 365;
        j_day_no = (j_day_no - 1) % 365;
    }

    int jm = 0;
    for (jm = 0; jm < 11 && j_day_no >= j_days_in_month[jm]; ++jm) {
        j_day_no -= j_days_in_month[jm];
    }
    j_y = jy;
    j_m = jm + 1;
    j_d = j_day_no + 1;
}

void CBasePanel::DrawLeftClockWidget(void)
{
    if (GameUI().IsInLevel() || g_hLoadingDialog.Get())
        return;

    int swide = 0, stall = 0;
    vgui2::surface()->GetScreenSize(swide, stall);
    if (swide <= 0 || stall <= 0)
        return;

    float scale = (float)stall / 600.0f;
    if (scale < 1.0f) scale = 1.0f;

    vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetDefaultScheme());
    if (!pScheme)
        return;

    vgui2::HFont hFont = pScheme->GetFont("Default", IsProportional());
    vgui2::HFont hFontBold = pScheme->GetFont("DefaultBold", IsProportional());
    vgui2::HFont hFontLarge = pScheme->GetFont("DefaultLarge", IsProportional());
    if (!hFont) hFont = hFontBold;
    if (!hFontLarge) hFontLarge = hFontBold ? hFontBold : hFont;
    if (!hFont) return;

    // Symmetrical positioning matching the right chat preview:
    int cardW = (int)(290.0f * scale + 0.5f);
    int cardH = (int)(84.0f * scale + 0.5f);
    int cardX = (int)(24.0f * scale + 0.5f);
    int cardY = (int)(75.0f * scale + 0.5f);

    // 1. Sleek frosted obsidian glass background (نامرئی‌طور و مدرن)
    vgui2::surface()->DrawSetColor(Color(12, 16, 24, 175));
    vgui2::surface()->DrawFilledRect(cardX, cardY, cardX + cardW, cardY + cardH);

    // 2. Cyan glowing outer border
    vgui2::surface()->DrawSetColor(Color(0, 205, 255, 80));
    vgui2::surface()->DrawOutlinedRect(cardX, cardY, cardX + cardW, cardY + cardH);

    // 3. Top accent highlight in emerald cyan
    vgui2::surface()->DrawSetColor(Color(0, 230, 210, 180));
    vgui2::surface()->DrawFilledRect(cardX, cardY, cardX + cardW, cardY + 2);

    int padX = (int)(14.0f * scale + 0.5f);
    int curY = cardY + (int)(8.0f * scale + 0.5f);

    // 4. Header: Live emerald indicator dot + "IRAN STANDARD TIME"
    int dotSize = std::max(5, (int)(5.0f * scale + 0.5f));
    vgui2::surface()->DrawSetColor(Color(0, 255, 180, 255));
    vgui2::surface()->DrawFilledRect(cardX + padX, curY + 3, cardX + padX + dotSize, curY + 3 + dotSize);

    wchar_t wHeader[] = L"IRAN STANDARD TIME";
    vgui2::surface()->DrawSetTextFont(hFontBold ? hFontBold : hFont);
    vgui2::surface()->DrawSetTextColor(Color(0, 220, 255, 240));
    vgui2::surface()->DrawSetTextPos(cardX + padX + dotSize + 6, curY);
    vgui2::surface()->DrawPrintText(wHeader, wcslen(wHeader));

    curY += (int)(18.0f * scale + 0.5f);

    // 5. Big Digital Clock & Persian Day of Week
    time_t now = time(nullptr);
    // Iran Standard Time (UTC+03:30 = +12600 seconds)
    time_t iranTime = now + 12600;
    struct tm tmIran;
    gmtime_s(&tmIran, &iranTime);

    wchar_t wClock[32]{};
    swprintf_s(wClock, L"%02d : %02d", tmIran.tm_hour, tmIran.tm_min);

    // Weekday in Persian
    // tm_wday: 0 = Sunday, 1 = Monday, 2 = Tuesday, 3 = Wednesday, 4 = Thursday, 5 = Friday, 6 = Saturday
    const wchar_t* kWeekDays[7] = {
        L"یک‌شنبه",   // 0: Sunday
        L"دوشنبه",     // 1: Monday
        L"سه‌شنبه",    // 2: Tuesday
        L"چهارشنبه",   // 3: Wednesday
        L"پنج‌شنبه",   // 4: Thursday
        L"جمعه",       // 5: Friday
        L"شنبه"        // 6: Saturday
    };

    int wdayIdx = tmIran.tm_wday;
    if (wdayIdx < 0 || wdayIdx > 6)
        wdayIdx = 0;

    std::wstring rawDay = kWeekDays[wdayIdx];
    std::wstring shapedDay = Persian::ShapeAndBiDi(rawDay);

    int clockW = 0, clockH = 0;
    vgui2::surface()->GetTextSize(hFontLarge, wClock, clockW, clockH);

    vgui2::surface()->DrawSetTextFont(hFontLarge);
    vgui2::surface()->DrawSetTextColor(Color(255, 220, 85, 255)); // Radiant Esports Gold
    vgui2::surface()->DrawSetTextPos(cardX + padX, curY);
    vgui2::surface()->DrawPrintText(wClock, wcslen(wClock));

    // Draw Day of Week Badge next to the digital clock
    int dayW = 0, dayH = 0;
    vgui2::surface()->GetTextSize(hFontBold ? hFontBold : hFont, shapedDay.c_str(), dayW, dayH);

    int dayBadgeX = cardX + padX + clockW + (int)(14.0f * scale + 0.5f);
    int badgePadX = (int)(7.0f * scale + 0.5f);
    int badgePadY = (int)(2.0f * scale + 0.5f);
    int badgeY = curY + (int)(3.0f * scale + 0.5f);

    vgui2::surface()->DrawSetColor(Color(0, 180, 255, 35));
    vgui2::surface()->DrawFilledRect(dayBadgeX - badgePadX, badgeY - badgePadY, dayBadgeX + dayW + badgePadX, badgeY + dayH + badgePadY);
    vgui2::surface()->DrawSetColor(Color(0, 205, 255, 110));
    vgui2::surface()->DrawOutlinedRect(dayBadgeX - badgePadX, badgeY - badgePadY, dayBadgeX + dayW + badgePadX, badgeY + dayH + badgePadY);

    vgui2::surface()->DrawSetTextFont(hFontBold ? hFontBold : hFont);
    vgui2::surface()->DrawSetTextColor(Color(0, 240, 255, 255)); // Vibrant Neon Cyan
    vgui2::surface()->DrawSetTextPos(dayBadgeX, badgeY);
    vgui2::surface()->DrawPrintText(shapedDay.c_str(), shapedDay.size());

    curY += (int)(23.0f * scale + 0.5f);

    // 6. Subtext: Accurate Jalali (Solar Hijri) Date + Gregorian Date
    int gy = tmIran.tm_year + 1900;
    int gm = tmIran.tm_mon + 1;
    int gd = tmIran.tm_mday;
    int jy = 0, jm = 0, jd = 0;
    ConvertGregorianToJalali(gy, gm, gd, jy, jm, jd);

    wchar_t wSub[64]{};
    swprintf_s(wSub, L"%04d/%02d/%02d  •  %04d/%02d/%02d", jy, jm, jd, gy, gm, gd);
    vgui2::surface()->DrawSetTextFont(hFont);
    vgui2::surface()->DrawSetTextColor(Color(130, 175, 205, 230));
    vgui2::surface()->DrawSetTextPos(cardX + padX, curY);
    vgui2::surface()->DrawPrintText(wSub, wcslen(wSub));
}

bool CBasePanel::IsMenuFading(void)
{
    return m_bFadingInMenus;
}

bool CBasePanel::IsInitialLoading(void)
{
    return m_bInitialLoading;
}

void CBasePanel::UpdateBackgroundState(void)
{
    const float alpha_trans_duration = 0.2f;

    GameConsole().SetParent(GetVPanel());

    if (GameUI().IsInLevel())
    {
        SetBackgroundRenderState(BACKGROUND_LEVEL);
    }
    else if (!m_bLevelLoading)
    {
        if (IsPC())
            SetBackgroundRenderState(BACKGROUND_MAINMENU);
    }
    else if (m_bLevelLoading && g_hLoadingDialog.Get())
    {
        SetBackgroundRenderState(BACKGROUND_LOADING);
    }
    else if (m_bEverActivated)
    {
        SetBackgroundRenderState(BACKGROUND_DISCONNECTED);
    }

    bool bHaveActiveDialogs = false;
    bool bIsInLevel = GameUI().IsInLevel();

    for (int i = 0; i < GetChildCount(); ++i)
    {
        vgui2::VPANEL child = vgui2::ipanel()->GetChild(GetVPanel(), i);
        const char *name = vgui2::ipanel()->GetName(child);

        if (child && vgui2::ipanel()->IsVisible(child) && vgui2::ipanel()->IsPopup(child) && child != m_pGameMenu->GetVPanel())
        {
            bHaveActiveDialogs = true;
            break;
        }
    }

    if (!bHaveActiveDialogs)
    {
        vgui2::VPANEL parent = GetVParent();

        for (int i = 0; i < vgui2::ipanel()->GetChildCount(parent); ++i)
        {
            vgui2::VPANEL child = vgui2::ipanel()->GetChild(parent, i);

            if (child && vgui2::ipanel()->IsVisible(child) && vgui2::ipanel()->IsPopup(child) && child != GetVPanel())
            {
                bHaveActiveDialogs = true;
                break;
            }
        }
    }

    bool bNeedDarkenedBackground = (bHaveActiveDialogs || bIsInLevel);

    if (m_bHaveDarkenedBackground != bNeedDarkenedBackground)
    {
        float targetAlpha, duration;

        if (bNeedDarkenedBackground || m_eBackgroundState == BACKGROUND_LOADING)
        {
            targetAlpha = m_BackdropColor[3];
            duration = m_flFrameFadeInTime;
        }
        else
        {
            targetAlpha = 0.0f;
            duration = alpha_trans_duration;
        }

        m_bHaveDarkenedBackground = bNeedDarkenedBackground;

        vgui2::GetAnimationController()->RunAnimationCommand(this, "m_flBackgroundFillAlpha", targetAlpha, 0.0f, duration, vgui2::AnimationController::INTERPOLATOR_LINEAR);
    }

    if (m_bLevelLoading)
        return;

    bool bNeedDarkenedTitleText = bHaveActiveDialogs;

    if (m_bHaveDarkenedTitleText != bNeedDarkenedTitleText || m_bForceTitleTextUpdate)
    {
        float targetTitleAlpha, duration;

        if (bHaveActiveDialogs || m_eBackgroundState == BACKGROUND_LOADING)
        {
            duration = m_flFrameFadeInTime;
            targetTitleAlpha = 128.0f;
        }
        else
        {
            duration = alpha_trans_duration;
            targetTitleAlpha = 255.0f;
        }

        if (m_pGameLogo)
            vgui2::GetAnimationController()->RunAnimationCommand(m_pGameLogo, "alpha", targetTitleAlpha, 0.0f, duration, vgui2::AnimationController::INTERPOLATOR_LINEAR);

        if (m_pGameMenu)
            vgui2::GetAnimationController()->RunAnimationCommand(m_pGameMenu, "alpha", targetTitleAlpha, 0.0f, duration, vgui2::AnimationController::INTERPOLATOR_LINEAR);

        for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
            vgui2::GetAnimationController()->RunAnimationCommand(m_pGameMenuButtons[i], "alpha", targetTitleAlpha, 0.0f, duration, vgui2::AnimationController::INTERPOLATOR_LINEAR);

        m_bFadingInMenus = false;
        m_bHaveDarkenedTitleText = bNeedDarkenedTitleText;
        m_bForceTitleTextUpdate = false;
    }
}

void CBasePanel::SetBackgroundRenderState(EBackgroundState state)
{
    if (state == m_eBackgroundState)
        return;

    float frametime = engine->GetClientTime();

    m_bRenderingBackgroundTransition = false;
    m_bFadingInMenus = false;

    if (state == BACKGROUND_EXITING)
    {
    }
    else if (state == BACKGROUND_DISCONNECTED || state == BACKGROUND_MAINMENU)
    {
        m_bFadingInMenus = true;
        m_flFadeMenuStartTime = frametime;
        m_flFadeMenuEndTime = frametime + 0.2f;

    }
    else if (state == BACKGROUND_LOADING)
    {
        SetMenuAlpha(0);

    }
    else if (state == BACKGROUND_LEVEL)
    {
        SetMenuAlpha(255);

    }

    m_eBackgroundState = state;
}

void CBasePanel::OnSizeChanged(int newWide, int newTall)
{
    // HACK HACK: We don't get any OnScreenSizeChanged updates on any component so we calling 
    // OnScreenSizeChanged in OnSizeChange event of the root panel that matches the actual viewport bounds

    BaseClass::OnScreenSizeChanged(newWide, newTall);
}

void CBasePanel::OnMousePressed(vgui2::MouseCode code)
{
    if (code == vgui2::MOUSE_LEFT && !GameUI().IsInLevel())
    {
        int cursorX = 0, cursorY = 0;
        vgui2::input()->GetCursorPos(cursorX, cursorY);
        ScreenToLocal(cursorX, cursorY);

        if (m_presenceBannerW > 0 &&
            cursorX >= m_presenceBannerX && cursorX <= (m_presenceBannerX + m_presenceBannerW) &&
            cursorY >= m_presenceBannerY && cursorY <= (m_presenceBannerY + m_presenceBannerH))
        {
            OnOpenLobbyChatDialog();
            return;
        }

        if (m_chatPreviewW > 0 &&
            cursorX >= m_chatPreviewX && cursorX <= (m_chatPreviewX + m_chatPreviewW) &&
            cursorY >= m_chatPreviewY && cursorY <= (m_chatPreviewY + m_chatPreviewH))
        {
            OnOpenLobbyChatDialog();
            return;
        }
    }
    BaseClass::OnMousePressed(code);
}

void CBasePanel::OnLevelLoadingStarted(const char *levelName)
{
    m_bLevelLoading = true;
    m_pGameMenu->ShowFooter(false);

    static char imageName[MAX_PATH];
    sprintf(imageName, "resource/maploading/loadingbg_%s", levelName);

    if (!g_hLoadingDialog.Get())
        g_hLoadingDialog = new CLoadingDialog(this);

    g_hLoadingDialog->SetBackgroundImage(imageName);
}

void CBasePanel::OnLevelLoadingFinished(void)
{
    m_bLevelLoading = false;
}

void CBasePanel::DrawBackgroundImage(void)
{
    int swide, stall;
    vgui2::surface()->GetScreenSize(swide, stall);

    int wide, tall;
    GetSize(wide, tall);

    float frametime = engine->GetAbsoluteTime();
    int alpha = 255;

    if (m_bRenderingBackgroundTransition)
    {
        alpha = (m_flTransitionEndTime - frametime) / (m_flTransitionEndTime - m_flTransitionStartTime) * 255;
        alpha = clamp(alpha, 0, 255);
    }

    int ypos = 0;

    float xScale, yScale;
    xScale = (float)swide / (float)m_iBaseResX;
    yScale = (float)stall / (float)m_iBaseResY;

    // iterate and draw all the background pieces
    for (int x = 0; x < m_ImageID.Size(); x++)
    {
        bimage_t &bimage = m_ImageID[x];

        int dx = bimage.x;
        int dy = bimage.y;
        int dw = bimage.x + bimage.width;
        int dt = bimage.y + bimage.height;

        if (bimage.scaled)
        {
            dx = (int)ceil(dx * xScale);
            dy = (int)ceil(dy * yScale);
            dw = (int)ceil(dw * xScale);
            dt = (int)ceil(dt * yScale);
        }

        // draw the color image only if the mono image isn't yet fully opaque
        vgui2::surface()->DrawSetColor(255, 255, 255, 255);
        vgui2::surface()->DrawSetTexture(bimage.imageID);
        vgui2::surface()->DrawTexturedRect(dx, dy, dw, dt);
    }


    if (IsPC() && (m_bRenderingBackgroundTransition || m_eBackgroundState == BACKGROUND_LOADING))
    {
        if (m_pGameMenu->GetAlpha() < 255)
        {
            vgui2::surface()->DrawSetColor(255, 255, 255, alpha);
            vgui2::surface()->DrawSetTexture(m_iLoadingImageID);

            int twide, ttall;
            vgui2::surface()->DrawGetTextureSize(m_iLoadingImageID, twide, ttall);
            vgui2::surface()->DrawTexturedRect(wide - twide, tall - ttall, wide, tall);
        }
    }

    if (m_bFadingInMenus)
    {
        alpha = (frametime - m_flFadeMenuStartTime) / (m_flFadeMenuEndTime - m_flFadeMenuStartTime) * 255;
        alpha = clamp(alpha, 0, 255);

        for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
            m_pGameMenuButtons[i]->SetAlpha(alpha);

        if (alpha == 255)
            m_bFadingInMenus = false;

        m_pGameMenu->SetAlpha(alpha);
    }
}

void CBasePanel::ApplyMultiplayerGameSettings()
{
    if (m_hCreateMultiplayerGameDialog)
        m_hCreateMultiplayerGameDialog->ApplyMultiplayerGameSettings();
}

void CBasePanel::CreateGameMenu(void)
{
    KeyValues *datafile = new KeyValues("GameMenu");
    //datafile->UsesEscapeSequences(true); // TODO not supported by sdk

    if (datafile->LoadFromFile(g_pFullFileSystem, "Resource/GameMenu.res"))
        m_pGameMenu = RecursiveLoadGameMenu(this, datafile);

    if (!m_pGameMenu)
    {
        Error(_T("Could not load file Resource/GameMenu.res"));
    }
    else
    {
        SETUP_PANEL(m_pGameMenu);
        m_pGameMenu->SetAlpha(0);
    }

    datafile->deleteThis();
}

void CBasePanel::CreateGameLogo(void)
{
    m_pGameLogo = new CMainMenuGameLogo(this, "BgMenuButton");
}

void CBasePanel::CreateBackGround(void)
{
}

void CBasePanel::CreateToolbar(void)
{
    int swide, stall;
    vgui2::surface()->GetScreenSize(swide, stall);

    m_pToolBar = new CToolBar(this, "ToolBar");
    m_pToolBar->SetZPos(-20);
    m_pToolBar->SetVisible(false);
    m_pToolBar->SetBounds(0, stall - m_iToolBarSize, swide, m_iToolBarSize);
}

void CBasePanel::UpdateGameMenus(void)
{
    // check our current state
    bool isInGame = GameUI().IsInLevel();
    bool isMulti = isInGame && (engine->GetMaxClients() > 1);

    // iterate all the menu items
    m_pGameMenu->UpdateMenuItemState(isInGame, isMulti);

    // position the menu
    InvalidateLayout();
    m_pGameMenu->SetVisible( true );
}

CGameMenu *CBasePanel::RecursiveLoadGameMenu(vgui2::Panel *parent, KeyValues *datafile)
{
    CGameMenu *menu = new CGameMenu(parent, datafile->GetName());

    for (KeyValues *dat = datafile->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
    {
        const char *label = dat->GetString("label", "<unknown>");
        const char *cmd = dat->GetString("command", NULL);
        const char *name = dat->GetString("name", label);

        // Keep nonessential online actions out of the client even when they are
        // supplied by an external GameMenu.res in the game installation.
        if (cmd && (!Q_stricmp(cmd, "ConnectToRandomServer") || !Q_stricmp(cmd, "OpenHelpUrl")))
            continue;

#if defined(GAMELAND_HOME_CLIENT) && GAMELAND_HOME_CLIENT
        // Home Client: completely remove New Game from main menu root
        if (cmd && !Q_stricmp(cmd, "OpenCreateMultiplayerGameDialog"))
            continue;
#endif

        // Never load blank separators or ghost buttons with empty label or command
        if (!cmd || !*cmd || !label || !*label)
            continue;

        menu->AddMenuItem(name, label, cmd, this, dat);
    }

    return menu;
}

void CBasePanel::RunFrame(void)
{
    PresenceClient::GetInstance().UpdateMainThreadState();

    if (!IsVisible())
        return;

    if (vgui2::surface()->GetModalPanel())
        vgui2::surface()->PaintTraverse(GetVPanel());

    vgui2::GetAnimationController()->UpdateAnimations(engine->GetAbsoluteTime());
    UpdateBackgroundState();
}

void CBasePanel::PerformLayout(void)
{
    BaseClass::PerformLayout();

    int wide, tall;
    vgui2::surface()->GetScreenSize(wide, tall);

    float scale = (float)tall / 600.0f;
    if (scale < 0.75f) scale = 0.75f;

    m_iGameMenuPos.x = (int)(20.0f * scale + 0.5f);
    m_iGameMenuInset = (int)(32.0f * scale + 0.5f);

    int menuWide, menuTall;
    m_pGameMenu->GetSize(menuWide, menuTall);

    int idealMenuY = tall - menuTall - m_iGameMenuInset;
    //int idealMenuY = tall / 2 - menuTall/2 - m_iGameMenuInset;
    int yDiff = idealMenuY - m_iGameMenuPos.y;

    for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
    {
        m_pGameMenuButtons[i]->SizeToContents();
        if (i < m_iGameTitlePos.Count())
            m_pGameMenuButtons[i]->SetPos(m_iGameTitlePos[i].x, m_iGameTitlePos[i].y + yDiff);
    }

    if (m_pGameLogo)
        m_pGameLogo->SetPos(m_iGameMenuPos.x + m_pGameLogo->GetOffsetX(), idealMenuY - m_pGameLogo->GetTall() + m_pGameLogo->GetOffsetY());

    m_pGameMenu->SetPos(m_iGameMenuPos.x, idealMenuY);

    if (m_bInitialLoading)
    {
        m_bInitialLoading = false;
        //GameConsole().CheckPending();

        engine->pfnClientCmd("mp3 loop media/gamestartup.mp3\n");
    }

    UpdateGameMenus();
}

void CBasePanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    m_iGameMenuInset = atoi(pScheme->GetResourceString("MainMenu.Inset"));
    m_iGameMenuInset *= 2;

    CUtlVector<Color> buttonColor;

    if (pScheme)
    {
        m_iGameTitlePos.RemoveAll();

        for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
        {
            m_pGameMenuButtons[i]->SetFont(pScheme->GetFont("TitleFont"));

            m_iGameTitlePos.AddToTail(coord());
            m_iGameTitlePos[i].x = vgui2::scheme()->GetProportionalScaledValue(53 + 50 * i);
            m_iGameTitlePos[i].y = vgui2::scheme()->GetProportionalScaledValue(190 + 17 * i);
            buttonColor.AddToTail(Color(255, 255, 255, 255));
        }

        m_iGameMenuPos.x = vgui2::scheme()->GetProportionalScaledValue(15);
        m_iGameMenuPos.y = vgui2::scheme()->GetProportionalScaledValue(240);
        m_iGameMenuInset = vgui2::scheme()->GetProportionalScaledValue(32);
    }
    else
    {
        for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
        {
            m_pGameMenuButtons[i]->SetFont(pScheme->GetFont("TitleFont"));

            buttonColor.AddToTail(Color(255, 255, 255, 255));
        }
    }

    for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
    {
        m_pGameMenuButtons[i]->SetDefaultColor(buttonColor[i], Color(0, 0, 0, 0));
        m_pGameMenuButtons[i]->SetArmedColor(buttonColor[i], Color(0, 0, 0, 0));
        m_pGameMenuButtons[i]->SetDepressedColor(buttonColor[i], Color(0, 0, 0, 0));
    }

    SetBgColor(Color(0, 0, 0, 0));

    m_flFrameFadeInTime = 0.3f;
    m_BackdropColor = Color(0, 0, 0, 128);

    int screenWide, screenTall;
    vgui2::surface()->GetScreenSize(screenWide, screenTall);

    float aspectRatio = (float)screenWide/(float)screenTall;
    bool bIsWidescreen = aspectRatio >= 1.5999f;

//    for (int y = 0; y < BACKGROUND_ROWS; y++)
//    {
//        for (int x = 0; x < BACKGROUND_COLUMNS; x++)
//        {
//            bimage_t &bimage = m_ImageID[y][x];
//            bimage.imageID = surface()->CreateNewTextureID();
//
//            char filename[MAX_PATH];
//            sprintf(filename, "resource/background/1024_%d_%c_BTE", y + 1, 'a' + x);
//            surface()->DrawSetTextureFile(bimage.imageID, filename, false, false);
//            surface()->DrawGetTextureSize(bimage.imageID, bimage.width, bimage.height);
//        }
//    }

//    m_iLoadingImageID = g_pVGuiSurface->CreateNewTextureID();
//    g_pVGuiSurface->DrawSetTextureFile(m_iLoadingImageID, "gfx/vgui/console/startup_loading", false, false);

    FileHandle_t file = g_pFullFileSystem->Open("resource/BackgroundLayout.txt", "rt");
    if (!file)
        return;

    int fileSize = g_pFullFileSystem->Size(file);
    char *buffer = (char *)alloca(fileSize + 1);
    g_pFullFileSystem->Read(buffer, fileSize, file);
    g_pFullFileSystem->Close(file);
    buffer[fileSize] = 0;

    //int vid_level;
    //gameuifuncs->GetCurrentRenderer(NULL, 0, NULL, NULL, NULL, &vid_level);

    char token[512];
    while (buffer && *buffer)
    {
        buffer = g_pFullFileSystem->ParseFile(buffer, token, NULL);
        if (!buffer || !buffer[0])
            break;

        if (!stricmp(token, "resolution"))
        {
            buffer = g_pFullFileSystem->ParseFile(buffer, token, NULL);
            m_iBaseResX = atoi(token);
            buffer = g_pFullFileSystem->ParseFile(buffer, token, NULL);
            m_iBaseResY = atoi(token);
        }
        else
        {
            bimage_t &bimage = m_ImageID[m_ImageID.AddToTail()];
            bimage.imageID = vgui2::surface()->CreateNewTextureID();

            char *ext = strstr(token, ".tga");
            if (ext)
                *ext = 0;

            vgui2::surface()->DrawSetTextureFile(bimage.imageID, token, 1, false);
            vgui2::surface()->DrawGetTextureSize(bimage.imageID, bimage.width, bimage.height);

            buffer = g_pFullFileSystem->ParseFile(buffer, token, NULL);
            bimage.scaled = stricmp(token, "scaled") == 0;
            buffer = g_pFullFileSystem->ParseFile(buffer, token, NULL);
            bimage.x = atoi(token);
            buffer = g_pFullFileSystem->ParseFile(buffer, token, NULL);
            bimage.y = atoi(token);
        }
    }
}

void CBasePanel::OnActivateModule(int moduleIndex)
{
}

void CBasePanel::OnGameUIActivated(void)
{
    if (!m_bEverActivated)
    {
        UpdateGameMenus();
        m_bEverActivated = true;
    }

    if (GameUI().IsInLevel())
    {
        // Entering the pause menu is a clean idle boundary. Stop or hide any
        // previously open UI work; a feature restarts only when explicitly
        // opened by the user from the pause menu.
        CloseBaseDialogs();
        OnCommand("OpenPauseMenu");
        m_pGameMenu->SetVisible(false);
    }
}

constexpr static char kConnectTo[] = "ConnectTo ";

void CBasePanel::RunMenuCommand(const char *command)
{
    if (!Q_stricmp(command, "OpenServerBrowser"))
    {
        OnOpenServerBrowser();
    }
    else if (!Q_stricmp(command, "OpenCreateMultiplayerGameDialog"))
    {
#if defined(GAMELAND_HOME_CLIENT) && GAMELAND_HOME_CLIENT
        return;
#else
        OnOpenCreateMultiplayerGameDialog();
#endif
    }
    else if (!Q_stricmp(command, "OpenOptionsDialog"))
    {
        OnOpenOptionsDialog();
    }
    else if (!Q_stricmp(command, "ResumeGame"))
    {
        engine->pfnClientCmd("cancelselect");
    }
    else if (!Q_stricmp(command, "Disconnect"))
    {
        engine->pfnClientCmd("disconnect\n");
    }
    else if (!Q_stricmp(command, "Quit"))
    {
        OnOpenQuitConfirmationDialog();
    }
    else if (!Q_stricmp(command, "QuitNoConfirm"))
    {
        SetVisible(false);
        vgui2::surface()->RestrictPaintToSinglePanel(GetVPanel());
        engine->pfnClientCmd("quit\n");
    }
    else if (!Q_stricmp(command, "ReleaseModalWindow"))
    {
        vgui2::surface()->RestrictPaintToSinglePanel(NULL);
    }
    else if (!Q_stricmp(command, "OpenPlayerListDialog"))
    {
        OnOpenPlayerListDialog();
    }
    else if (!Q_stricmp(command, "OpenOnlinePlayers") || !Q_stricmp(command, "OpenPlayersList"))
    {
        OnOpenOnlinePlayersDialog();
    }
    else if (!Q_stricmp(command, "OpenLobbyChat") || !Q_stricmp(command, "OpenGlobalChat"))
    {
        OnOpenLobbyChatDialog();
    }
    else if (!Q_stricmp(command, "OpenDemoUploader") || !Q_stricmp(command, "OpenDemoStudio"))
    {
        OnOpenDemoUploaderDialog();
    }
    else if (!Q_strncmp(command, kConnectTo, sizeof(kConnectTo) - 1))
    {
        std::vector<std::string> args;
        nitro_utils::split_in_args(command, args, 2);

        if (args.size() != 2)
            return;

        auto& address = args[1];

        if (std::find_if(address.cbegin(), address.cend(), [](char ch) { return ch == ' ' || ch == '\n' || ch == '\r' || ch == '"' || ch == '\''; }) != address.cend())
            return;

        engine->pfnClientCmd(std::format("connect {}\n", address).c_str());
    }
    else if (GameUINext().InvokeRunMenuCommand(command))
    {
        // command handled
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

void CBasePanel::OnKeyCodePressed(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_F4)
    {
        OnOpenDemoUploaderDialog();
        return;
    }
    BaseClass::OnKeyCodePressed(code);
}

void CBasePanel::OnCommand(const char *command)
{
    RunMenuCommand(command);
}

void CBasePanel::RunAnimationWithCallback(vgui2::Panel *parent, const char *animName, KeyValues *msgFunc)
{
    if (!m_pConsoleAnimationController)
        return;

    m_pConsoleAnimationController->StartAnimationSequence(animName);

    float sequenceLength = m_pConsoleAnimationController->GetAnimationSequenceLength(animName);

    if (sequenceLength)
        sequenceLength += g_flAnimationPadding;

    if (parent && msgFunc)
        PostMessage(parent, msgFunc, sequenceLength);
}

class CQuitQueryBox : public vgui2::QueryBox
{
    DECLARE_CLASS_SIMPLE(CQuitQueryBox, vgui2::QueryBox);

public:
    CQuitQueryBox(const char *title, const char *info, Panel *parent) : BaseClass(title, info, parent)
    {
    }

    void DoModal(Frame *pFrameOver)
    {
        BaseClass::DoModal(pFrameOver);
        vgui2::surface()->RestrictPaintToSinglePanel(GetVPanel());
    }

    void OnKeyCodePressed(vgui2::KeyCode code)
    {
        if (code == vgui2::KeyCode::KEY_ESCAPE)
        {
            SetAlpha(0);
            Close();
        }
        else
            BaseClass::OnKeyCodePressed(code);
    }

    virtual void OnClose(void)
    {
        BaseClass::OnClose();
        vgui2::surface()->RestrictPaintToSinglePanel(NULL);
    }
};

void CBasePanel::OnOpenQuitConfirmationDialog(void)
{
    if (!m_hQuitQueryBox.Get())
    {
        m_hQuitQueryBox = new CQuitQueryBox("#GameUI_QuitConfirmationTitle", "#GameUI_QuitConfirmationText", this);
        m_hQuitQueryBox->SetOKButtonText("#GameUI_Quit");
        m_hQuitQueryBox->SetOKCommand(new KeyValues("Command", "command", "QuitNoConfirm"));
        m_hQuitQueryBox->SetCancelCommand(new KeyValues("Command", "command", "ReleaseModalWindow"));
        m_hQuitQueryBox->AddActionSignalTarget(this);
        m_hQuitQueryBox->DoModal();
    }
}

void CBasePanel::OnOpenServerBrowser(void)
{
    GameUI().ActivateServerBrowser();
}

void CBasePanel::OnOpenOptionsDialog(const char* tabName)
{
    if (!m_hOptionsDialog.Get())
    {
        m_hOptionsDialog = new COptionsDialog(this);
        PositionDialog(m_hOptionsDialog);
    }

    m_hOptionsDialog->Activate();

    if (tabName != nullptr)
    {
        m_hOptionsDialog->OpenTab(tabName);
    }
}

void CBasePanel::OnOpenPlayerListDialog()
{
    if (!m_hPlayerListDialog.Get())
    {
        m_hPlayerListDialog = new CPlayerListDialog(this);
        PositionDialog(m_hPlayerListDialog);
    }
    m_hPlayerListDialog->Activate();
}

void CBasePanel::OnOpenCreateMultiplayerGameDialog(void)
{
#if defined(GAMELAND_HOME_CLIENT) && GAMELAND_HOME_CLIENT
    return;
#else
    if (!m_hCreateMultiplayerGameDialog.Get())
    {
        m_hCreateMultiplayerGameDialog = new CCreateMultiplayerGameDialog(this);
        PositionDialog(m_hCreateMultiplayerGameDialog);
    }

    m_hCreateMultiplayerGameDialog->Activate();
#endif
}

void CBasePanel::OnOpenDemoUploaderDialog(void)
{
    if (!m_hDemoUploaderDialog.Get())
    {
        m_hDemoUploaderDialog = new CDemoUploaderDialog(this);
        PositionDialog(m_hDemoUploaderDialog);
    }

    m_hDemoUploaderDialog->Activate();
}

void CBasePanel::OnOpenOnlinePlayersDialog(void)
{
    if (!m_hOnlinePlayersDialog.Get())
    {
        m_hOnlinePlayersDialog = new COnlinePlayersDialog(this);
        PositionDialog(m_hOnlinePlayersDialog);
    }

    m_hOnlinePlayersDialog->Activate();
}

void CBasePanel::OnOpenLobbyChatDialog(void)
{
    if (!m_hLobbyChatDialog.Get())
    {
        m_hLobbyChatDialog = new CLobbyChatDialog(this);
        PositionDialog(m_hLobbyChatDialog);
    }

    m_hLobbyChatDialog->Activate();
}

void CBasePanel::PositionDialog(vgui2::PHandle dlg)
{
    if (!dlg.Get())
        return;

    int x, y, ww, wt, wide, tall;
    vgui2::surface()->GetWorkspaceBounds(x, y, ww, wt);
    dlg->GetSize(wide, tall);
    dlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

void CBasePanel::PositionDialog(vgui2::Panel *pdlg)
{
    int x, y, ww, wt, wide, tall;
    vgui2::surface()->GetWorkspaceBounds(x, y, ww, wt);
    pdlg->GetSize(wide, tall);
    pdlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

void CBasePanel::OnGameUIHidden(void)
{
    if (m_hOptionsDialog.Get())
        PostMessage(m_hOptionsDialog.Get(), new KeyValues("GameUIHidden"));
}

void CBasePanel::SetMenuAlpha(int alpha)
{
    m_pGameMenu->SetAlpha(alpha);

    if (m_pGameLogo)
        m_pGameLogo->SetAlpha(alpha);

    for (int i = 0; i < m_pGameMenuButtons.Count(); ++i)
        m_pGameMenuButtons[i]->SetAlpha(alpha);

    m_bForceTitleTextUpdate = true;
}

void CBasePanel::SetMenuItemBlinkingState(const char *itemName, bool state)
{
    for (int i = 0; i < GetChildCount(); i++)
    {
        vgui2::Panel *child = GetChild(i);
        CGameMenu *pGameMenu = dynamic_cast<CGameMenu *>(child);

        if (pGameMenu)
            pGameMenu->SetMenuItemBlinkingState(itemName, state);
    }
}

void CBasePanel::RunEngineCommand(const char *command)
{
    engine->pfnClientCmd((char *)command);
}

void CBasePanel::RunCloseAnimation(const char *animName)
{
    RunAnimationWithCallback(this, animName, new KeyValues("FinishDialogClose"));
}

void CBasePanel::FinishDialogClose(void)
{
}

CFooterPanel::CFooterPanel(vgui2::Panel *parent, const char *panelName) : BaseClass(parent, panelName)
{
    SetVisible(true);
    SetAlpha(0);

    m_pHelpName = NULL;
    m_pSizingLabel = new vgui2::Label(this, "SizingLabel", "");
    m_pSizingLabel->SetVisible(false);

    m_nButtonGap = 32;
    m_nButtonGapDefault = 32;
    m_ButtonPinRight = 100;
    m_FooterTall = 80;

    int wide, tall;
    vgui2::surface()->GetScreenSize(wide, tall);

    if (tall <= 480)
        m_FooterTall = 60;

    m_ButtonOffsetFromTop = 0;
    m_ButtonSeparator = 4;
    m_TextAdjust = 0;

    m_bPaintBackground = false;
    m_bCenterHorizontal = false;

    m_szButtonFont[0] = '\0';
    m_szTextFont[0] = '\0';
    m_szFGColor[0] = '\0';
    m_szBGColor[0] = '\0';
}

CFooterPanel::~CFooterPanel(void)
{
    SetHelpNameAndReset(NULL);

    delete m_pSizingLabel;
}

void CFooterPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    m_hButtonFont = pScheme->GetFont((m_szButtonFont[0] != '\0') ? m_szButtonFont : "GameUIButtons");
    m_hTextFont = pScheme->GetFont((m_szTextFont[0] != '\0') ? m_szTextFont : "MenuLarge");

    SetFgColor(pScheme->GetColor(m_szFGColor, Color(255, 255, 255, 255)));
    SetBgColor(pScheme->GetColor(m_szBGColor, Color(0, 0, 0, 255)));

    int x, y, w, h;
    GetParent()->GetBounds(x, y, w, h);
    SetBounds(x, h - m_FooterTall, w, m_FooterTall);
}

void CFooterPanel::ApplySettings(KeyValues *inResourceData)
{
    BaseClass::ApplySettings(inResourceData);

    m_nButtonGap = inResourceData->GetInt("buttongap", 32);
    m_nButtonGapDefault = m_nButtonGap;
    m_ButtonPinRight = inResourceData->GetInt("button_pin_right", 100);
    m_FooterTall = inResourceData->GetInt("tall", 80);
    m_ButtonOffsetFromTop = inResourceData->GetInt("buttonoffsety", 0);
    m_ButtonSeparator = inResourceData->GetInt("button_separator", 4);
    m_TextAdjust = inResourceData->GetInt("textadjust", 0);

    m_bCenterHorizontal = (inResourceData->GetInt("center", 0) == 1);
    m_bPaintBackground = (inResourceData->GetInt("paintbackground", 0) == 1);

    Q_strncpy(m_szTextFont, inResourceData->GetString("fonttext", "MenuLarge"), sizeof(m_szTextFont));
    Q_strncpy(m_szButtonFont, inResourceData->GetString("fontbutton", "GameUIButtons"), sizeof(m_szButtonFont));

    Q_strncpy(m_szFGColor, inResourceData->GetString("fgcolor", "White"), sizeof(m_szFGColor));
    Q_strncpy(m_szBGColor, inResourceData->GetString("bgcolor", "Black"), sizeof(m_szBGColor));

    for (KeyValues *pButton = inResourceData->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey())
    {
        const char *pName = pButton->GetName();

        if (!Q_stricmp(pName, "button"))
        {
            const char *pText = pButton->GetString("text", "NULL");
            const char *pIcon = pButton->GetString("icon", "NULL");
            AddNewButtonLabel(pText, pIcon);
        }
    }

    InvalidateLayout(false, true);
}

void CFooterPanel::AddButtonsFromMap(vgui2::Frame *pMenu)
{
    CControllerMap *pMap = dynamic_cast<CControllerMap *>(pMenu->FindChildByName("ControllerMap"));

    if (pMap)
    {
        int buttonCt = pMap->NumButtons();

        for (int i = 0; i < buttonCt; ++i)
        {
            const char *pText = pMap->GetBindingText(i);

            if (pText)
                AddNewButtonLabel(pText, pMap->GetBindingIcon(i));
        }
    }

    SetHelpNameAndReset(pMenu->GetName());
}

void CFooterPanel::SetStandardDialogButtons(void)
{
    SetHelpNameAndReset("Dialog");
    AddNewButtonLabel("#GameUI_Action", "#GameUI_Icons_A_BUTTON");
    AddNewButtonLabel("#GameUI_Close", "#GameUI_Icons_B_BUTTON");
}

void CFooterPanel::SetHelpNameAndReset(const char *pName)
{
    if (m_pHelpName)
    {
        free(m_pHelpName);
        m_pHelpName = NULL;
    }

    if (pName)
        m_pHelpName = strdup(pName);

    ClearButtons();
}

const char *CFooterPanel::GetHelpName(void)
{
    return m_pHelpName;
}

void CFooterPanel::ClearButtons(void)
{
    m_ButtonLabels.PurgeAndDeleteElements();
}

void CFooterPanel::AddNewButtonLabel(const char *text, const char *icon)
{
    ButtonLabel_t *button = new ButtonLabel_t;

    Q_strncpy(button->name, text, MAX_PATH);
    button->bVisible = true;

    wchar_t *pIcon = g_pVGuiLocalize->Find(icon);

    if (pIcon)
    {
        button->icon[0] = pIcon[0];
        button->icon[1] = '\0';
    }
    else
        button->icon[0] = '\0';

    wchar_t *pText = g_pVGuiLocalize->Find(text);

    if (pText)
        wcsncpy(button->text, pText, wcslen(pText) + 1);
    else
        button->text[0] = '\0';

    m_ButtonLabels.AddToTail(button);
}

void CFooterPanel::ShowButtonLabel(const char *name, bool show)
{
    for (int i = 0; i < m_ButtonLabels.Count(); ++i)
    {
        if (!Q_stricmp(m_ButtonLabels[i]->name, name))
        {
            m_ButtonLabels[i]->bVisible = show;
            break;
        }
    }
}

void CFooterPanel::SetButtonText(const char *buttonName, const char *text)
{
    for (int i = 0; i < m_ButtonLabels.Count(); ++i)
    {
        if (!Q_stricmp(m_ButtonLabels[i]->name, buttonName))
        {
            wchar_t *wtext = g_pVGuiLocalize->Find(text);

            if (text)
                wcsncpy(m_ButtonLabels[i]->text, wtext, wcslen(wtext) + 1);
            else
                m_ButtonLabels[i]->text[0] = '\0';

            break;
        }
    }
}

void CFooterPanel::PaintBackground(void)
{
    if (!m_bPaintBackground)
        return;

    BaseClass::PaintBackground();
}

void CFooterPanel::Paint(void)
{
    int wide = GetWide();
    int right = wide - m_ButtonPinRight;

    int buttonHeight = vgui2::surface()->GetFontTall(m_hButtonFont);
    int fontHeight = vgui2::surface()->GetFontTall(m_hTextFont);
    int textY = (buttonHeight - fontHeight) / 2 + m_TextAdjust;

    if (textY < 0)
        textY = 0;

    int y = m_ButtonOffsetFromTop;

    if (!m_bCenterHorizontal)
    {
        int x = right;

        for (int i = 0; i < m_ButtonLabels.Count(); ++i)
        {
            ButtonLabel_t *pButton = m_ButtonLabels[i];

            if (!pButton->bVisible)
                continue;

            m_pSizingLabel->SetFont(m_hTextFont);
            m_pSizingLabel->SetText(pButton->text);
            m_pSizingLabel->SizeToContents();

            int iTextWidth = m_pSizingLabel->GetWide();

            if (iTextWidth == 0)
                x += m_nButtonGap;
            else
                x -= iTextWidth;

            vgui2::surface()->DrawSetTextFont(m_hTextFont);
            vgui2::surface()->DrawSetTextColor(GetFgColor());
            vgui2::surface()->DrawSetTextPos(x, y + textY);
            vgui2::surface()->DrawPrintText(pButton->text, wcslen(pButton->text));

            x -= (vgui2::surface()->GetCharacterWidth(m_hButtonFont, pButton->icon[0]) + m_ButtonSeparator);

            vgui2::surface()->DrawSetTextFont(m_hButtonFont);
            vgui2::surface()->DrawSetTextColor(255, 255, 255, 255);
            vgui2::surface()->DrawSetTextPos(x, y);
            vgui2::surface()->DrawPrintText(pButton->icon, 1);

            x -= m_nButtonGap;
        }
    }
    else
    {
        int x = wide / 2;
        int totalWidth = 0;
        int i = 0;
        int nButtonCount = 0;

        for (i = 0; i < m_ButtonLabels.Count(); ++i)
        {
            ButtonLabel_t *pButton = m_ButtonLabels[i];

            if (!pButton->bVisible)
                continue;

            m_pSizingLabel->SetFont(m_hTextFont);
            m_pSizingLabel->SetText(pButton->text);
            m_pSizingLabel->SizeToContents();

            totalWidth += vgui2::surface()->GetCharacterWidth(m_hButtonFont, pButton->icon[0]);
            totalWidth += m_ButtonSeparator;
            totalWidth += m_pSizingLabel->GetWide();

            nButtonCount++;
        }

        totalWidth += (nButtonCount - 1) * m_nButtonGap;
        x -= (totalWidth / 2);

        for (i = 0; i < m_ButtonLabels.Count(); ++i)
        {
            ButtonLabel_t *pButton = m_ButtonLabels[i];

            if (!pButton->bVisible)
                continue;

            m_pSizingLabel->SetFont(m_hTextFont);
            m_pSizingLabel->SetText(pButton->text);
            m_pSizingLabel->SizeToContents();

            int iTextWidth = m_pSizingLabel->GetWide();

            vgui2::surface()->DrawSetTextFont(m_hButtonFont);
            vgui2::surface()->DrawSetTextColor(255, 255, 255, 255);
            vgui2::surface()->DrawSetTextPos(x, y);
            vgui2::surface()->DrawPrintText(pButton->icon, 1);

            x += vgui2::surface()->GetCharacterWidth(m_hButtonFont, pButton->icon[0]) + m_ButtonSeparator;

            vgui2::surface()->DrawSetTextFont(m_hTextFont);
            vgui2::surface()->DrawSetTextColor(GetFgColor());
            vgui2::surface()->DrawSetTextPos(x, y + textY);
            vgui2::surface()->DrawPrintText(pButton->text, wcslen(pButton->text));

            x += iTextWidth + m_nButtonGap;
        }
    }
}

DECLARE_BUILD_FACTORY(CFooterPanel);

CMainMenuGameLogo::CMainMenuGameLogo(vgui2::Panel *parent, const char *name) : vgui2::EditablePanel(parent, name)
{
    m_nOffsetX = 0;
    m_nOffsetY = 0;
}

void CMainMenuGameLogo::ApplySettings(KeyValues *inResourceData)
{
    BaseClass::ApplySettings(inResourceData);

    m_nOffsetX = inResourceData->GetInt("offsetX", 0);
    m_nOffsetY = inResourceData->GetInt("offsetY", 0);
}

void CMainMenuGameLogo::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    LoadControlSettings("Resource/GameLogo.res");
}

void CBasePanel::CloseBaseDialogs(void)
{
    GameConsole().Hide();

    if (m_hOptionsDialog.Get())
        m_hOptionsDialog->Close();

    if (m_hCreateMultiplayerGameDialog.Get())
        m_hCreateMultiplayerGameDialog->Close();

    if (m_hPlayerListDialog.Get())
        m_hPlayerListDialog->Close();

    if (m_hDemoUploaderDialog.Get())
        m_hDemoUploaderDialog->Close();

    if (CServerBrowserDialog::GetInstance())
    {
        // "Close" is sent first so both LAN and Online pages cancel their
        // outstanding refresh/query work before the window is hidden.
        ServerBrowserDialog().OnCommand("Close");
        ServerBrowserDialog().Close();
    }
}
