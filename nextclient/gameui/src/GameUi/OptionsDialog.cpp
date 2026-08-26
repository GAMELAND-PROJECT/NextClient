#include "OptionsDialog.h"

#include <GameUi.h>

#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/QueryBox.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurfaceNext.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "OptionsSubMultiplayer.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVoice.h"
#include "OptionsSubVideo.h"

#include "ModInfo.h"

#include "KeyValues.h"

#include <algorithm>

#undef PostMessage

COptionsDialog::COptionsDialog(vgui2::Panel *parent) : PropertyDialog(parent, "OptionsDialog")
{
    // Give all localized tab labels and page content enough room on modern
    // displays while preserving the fixed-layout behavior of GoldSrc VGUI.
    SetBounds(0, 0, 640, 450);
    SetSizeable(false);
    SetTitle("#GameUI_Options", true);

    m_pOptionsSubMultiplayer = NULL;
    m_pOptionsSubKeyboard = NULL;
    m_pOptionsSubMouse = NULL;
    m_pOptionsSubAudio = NULL;
    m_pOptionsSubVideo = NULL;
    m_pOptionsSubVoice = NULL;

    if (!ModInfo().IsSinglePlayerOnly())
        m_pOptionsSubMultiplayer = new COptionsSubMultiplayer(this);

    m_pOptionsSubKeyboard = new COptionsSubKeyboard(this);
    m_pOptionsSubMouse = new COptionsSubMouse(this);
    m_pOptionsSubAudio = new COptionsSubAudio(this);
    m_pOptionsSubVideo = new COptionsSubVideo(this);

    if (!ModInfo().IsSinglePlayerOnly())
    {
        m_pOptionsSubVoice = new COptionsSubVoice(this);
    }


    if (m_pOptionsSubMultiplayer)
        AddPage(m_pOptionsSubMultiplayer, "#GameUI_Multiplayer");
    AddPage(m_pOptionsSubKeyboard, "#GameUI_Keyboard");
    AddPage(m_pOptionsSubMouse, "#GameUI_Mouse");
    AddPage(m_pOptionsSubAudio, "#GameUI_Audio");
    AddPage(m_pOptionsSubVideo, "#GameUI_Video");
    if (m_pOptionsSubVoice)
        AddPage(m_pOptionsSubVoice, "#GameUI_Voice");

    if (m_pOptionsSubMultiplayer)
        m_tabNames.Insert("multiplayer", m_pOptionsSubMultiplayer);
    m_tabNames.Insert("keyboard", m_pOptionsSubKeyboard);
    m_tabNames.Insert("mouse", m_pOptionsSubMouse);
    m_tabNames.Insert("audio", m_pOptionsSubAudio);
    m_tabNames.Insert("video", m_pOptionsSubVideo);
    if (m_pOptionsSubVoice)
        m_tabNames.Insert("voice", m_pOptionsSubVoice);
    SetApplyButtonVisible(true);
    // The stock GoldSrc PropertySheet only supports horizontal tabs. They are
    // hidden in PerformLayout and replaced by an Options-only vertical rail.

    if (m_pOptionsSubMultiplayer)
        AddNavigationButton("multiplayer", "#GameUI_Multiplayer");
    AddNavigationButton("keyboard", "#GameUI_Keyboard");
    AddNavigationButton("mouse", "#GameUI_Mouse");
    AddNavigationButton("audio", "#GameUI_Audio");
    AddNavigationButton("video", "#GameUI_Video");
    if (m_pOptionsSubVoice)
        AddNavigationButton("voice", "#GameUI_Voice");

    UpdateNavigationState();
    UpdateResponsiveBounds();
}

COptionsDialog::~COptionsDialog(void)
{
}

void COptionsDialog::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_PAD_ENTER)
        code = vgui2::KEY_ENTER;

    if (!GameUI().IsInLevel() && code == vgui2::KEY_ESCAPE)
    {
        Close();
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void COptionsDialog::Activate(void)
{
    bool was_visible = IsVisible();

    UpdateResponsiveBounds();

    BaseClass::Activate();

    if (!was_visible)
    {
        // Preserve the last active tab when the user reopens Options.
        ResetAllData();
        EnableApplyButton(false);
        InvalidateLayout(true);
    }
}

void COptionsDialog::OpenTab(const char* tabName) {
    int index = m_tabNames.Find(tabName);
    
    if(index != m_tabNames.InvalidIndex())
    {
        auto page = m_tabNames[index];
        if (GetActivePage() != page)
            GetPropertySheet()->SetActivePage(page);
        UpdateNavigationState();
        InvalidateLayout(true);
    }
}

void COptionsDialog::UpdateResponsiveBounds()
{
    int screenWide = 640;
    int screenTall = 480;
    vgui2::surface()->GetScreenSize(screenWide, screenTall);

    // Keep the dialog comfortably inside 640x480 while avoiding the large,
    // mostly empty canvas produced on modern desktop resolutions.
    const int dialogWide = std::max(600, std::min(620, screenWide - 12));
    const int dialogTall = std::max(392, std::min(408, screenTall - 28));
    SetSize(dialogWide, dialogTall);
    MoveToCenterOfScreen();
    InvalidateLayout(true);
}

void COptionsDialog::AddNavigationButton(const char* tabName, const char* label)
{
    vgui2::Button* button = new vgui2::Button(this, tabName, label, this);
    button->SetCommand(tabName);
    m_navigationButtons.AddToTail(button);
}

void COptionsDialog::UpdateNavigationState()
{
    for (int i = 0; i < m_navigationButtons.Count(); ++i)
    {
        vgui2::Button* button = m_navigationButtons[i];
        const int index = m_tabNames.Find(button->GetName());
        button->SetEnabled(index == m_tabNames.InvalidIndex() ||
                           m_tabNames[index] != GetActivePage());
    }
}

void COptionsDialog::OnCommand(const char* command)
{
    if (m_tabNames.Find(command) != m_tabNames.InvalidIndex())
    {
        OpenTab(command);
        return;
    }

    BaseClass::OnCommand(command);
}

void COptionsDialog::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide, tall;
    GetSize(wide, tall);
    const int navigationLeft = 8;
    const int navigationWidth = 86;
    const int navigationTop = 32;
    const int navigationButtonHeight = 25;
    const int navigationGap = 4;

    for (int i = 0; i < m_navigationButtons.Count(); ++i)
    {
        m_navigationButtons[i]->SetBounds(
            navigationLeft,
            navigationTop + i * (navigationButtonHeight + navigationGap),
            navigationWidth,
            navigationButtonHeight);
    }

    const int contentLeft = navigationLeft + navigationWidth + 8;
    const int contentWide = wide - contentLeft - 10;
    const int contentTall = tall - 82;
    vgui2::PropertySheet* sheet = GetPropertySheet();
    sheet->SetBounds(contentLeft, 25, contentWide, contentTall);
    sheet->InvalidateLayout(true);

    // PropertySheet makes its private PageTab children visible during every
    // layout pass, so hide them after that pass and let the active page use
    // the full content area without the obsolete horizontal tab row.
    for (int i = 0; i < sheet->GetChildCount(); ++i)
    {
        vgui2::Panel* child = sheet->GetChild(i);
        if (child && !Q_stricmp(child->GetName(), "tab"))
            child->SetVisible(false);
    }

    if (vgui2::Panel* page = sheet->GetActivePage())
    {
        page->SetBounds(0, 0, contentWide, contentTall);
        // Pages with responsive child layouts (notably Keyboard) must be laid
        // out after PropertySheet has assigned their final content bounds.
        page->InvalidateLayout(true);
    }
}

void COptionsDialog::OnClose(void)
{
    BaseClass::OnClose();
}

void COptionsDialog::OnGameUIHidden(void)
{
    for (int i = 0; i < GetChildCount(); i++)
    {
        Panel *pChild = GetChild(i);

        if (pChild)
            PostMessage(pChild, new KeyValues("GameUIHidden"));
    }
}
