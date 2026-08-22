#include "GameUi.h"
//#include "BaseUI.h"
#include "OptionsSubVideo.h"
#include "CvarSlider.h"
#include "CvarToggleCheckButton.h"
#include "igameuifuncs.h"
//#include "modes.h"

#include <vgui_controls/Tooltip.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#pragma warning(disable: 4101)

inline bool IsWideScreen(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    // Treat 3:2 and anything wider as widescreen. Integer arithmetic avoids
    // fragile exact floating-point comparisons and includes 1366x768,
    // 16:10, 16:9 and ultrawide display modes.
    return static_cast<long long>(width) * 2 >= static_cast<long long>(height) * 3;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::COptionsSubVideo(vgui2::Panel *parent) : PropertyPage(parent, NULL)
{
    m_pUserConfig = std::make_shared<nitro_utils::FileConfigProvider>("user_game_config.ini");

    memset( &m_OrigSettings, 0, sizeof( m_OrigSettings ) );
    memset( &m_CurrentSettings, 0, sizeof( m_CurrentSettings ) );

    m_pBrightnessSlider = new CCvarSlider( this, "Brightness", "#GameUI_Brightness",
                                           0.0f, 4.0f, "brightness" );

    m_pGammaSlider = new CCvarSlider( this, "Gamma", "#GameUI_Gamma",
                                      1.0f, 4.0f, "gamma" );

    GetVidSettings();

    m_pMode = new vgui2::ComboBox(this, "Resolution", 6, false);

    m_pAspectRatio = new vgui2::ComboBox( this, "AspectRatio", 2, false );

    m_pVsync = new CCvarToggleCheckButton( this, "VSync", "#GameUI_VSync", "gl_vsync" );
    m_pVsync->SetVisible(true);

    wchar_t *unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectNormal");
    g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, m_pszAspectName[0], 32);
    unicodeText = g_pVGuiLocalize->Find("#GameUI_AspectWide");
    g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeText, m_pszAspectName[1], 32);

    int iNormalItemID = m_pAspectRatio->AddItem( m_pszAspectName[0], NULL );
    int iWideItemID = m_pAspectRatio->AddItem( m_pszAspectName[1], NULL );

    m_bStartWidescreen = IsWideScreen( m_CurrentSettings.w, m_CurrentSettings.h );
    if ( m_bStartWidescreen )
    {
        m_pAspectRatio->ActivateItem( iWideItemID );
    }
    else
    {
        m_pAspectRatio->ActivateItem( iNormalItemID );
    }

    m_pWindowed = new vgui2::CheckButton( this, "Windowed", "#GameUI_Windowed" );
    m_pWindowed->SetSelected(m_CurrentSettings.windowed != 0);
    m_pWindowed->SetVisible(true);

    m_pHDModels = new vgui2::CheckButton( this, "HDModels", "#GameUI_HDModels" );
    m_pHDModels->SetSelected(m_CurrentSettings.hdmodels != 0);
    m_pHDModels->SetVisible(true);

    m_pHighVideoQuality = new vgui2::CheckButton(this, "HighVideoQuality", "High Video Quality");
    m_pHighVideoQuality->SetSelected(m_CurrentSettings.vid_level > 0);
    m_pHighVideoQuality->SetVisible(true);

    m_pDisableMultitexture = new vgui2::CheckButton( this, "DisableMultitexture", "#GameUI_DisableMultitexture" );
    m_pDisableMultitexture->SetSelected(m_CurrentSettings.disable_multitexture != 0);
    m_pDisableMultitexture->SetVisible(true);
    m_pDisableMultitexture->GetTooltip()->SetTooltipFormatToNoWrap();
    m_pDisableMultitexture->GetTooltip()->SetText("#GameUI_DisableMultitexture_Tooltip");

    m_pStretchAspect = new vgui2::CheckButton( this, "StretchAspect", "#GameUI_StretchAspect" );
    m_pStretchAspect->SetSelected(m_CurrentSettings.stretch_aspect != 0);
    m_pStretchAspect->SetVisible(true);

    LoadControlSettings("Resource\\OptionsSubVideo.res");
    PrepareResolutionList();
}

void COptionsSubVideo::PrepareResolutionList( void )
{
    vmode_t *plist = NULL;
    int count = 0;
    bool foundWidescreen = false;
    bool foundNormal = false;
    int nItemsAdded = 0;

    g_pGameUIFuncs->GetVideoModes( &plist, &count );

    // Get selected resolution in list (not current game resolution)
    vmode_t lastSelectedResolution{};
    lastSelectedResolution.iWidth = m_CurrentSettings.w;
    lastSelectedResolution.iHeight = m_CurrentSettings.h;
    GetSelectedResolution(lastSelectedResolution.iWidth, lastSelectedResolution.iHeight);

    // Clean up before filling the info again.
    m_pMode->DeleteAllItems();

    int selectedItemID = -1;
    int nearestItemID = -1;
    int minimumResolutionDistance = INT_MAX;
    for (int i = 0; i < count; i++, plist++)
    {
        // exclude obscenely small resolutions :)
        if (plist->iWidth < 640 || plist->iHeight < 480)
            continue;

        const bool isWidescreen = IsWideScreen(plist->iWidth, plist->iHeight);
        if (isWidescreen)
            foundWidescreen = true;
        else
            foundNormal = true;

        char sz[ 256 ];
        sprintf( sz, "%i x %i", plist->iWidth, plist->iHeight );

        int itemID = -1;
        if (isWidescreen)
        {
            if (m_bStartWidescreen)
            {
                itemID = m_pMode->AddItem( sz, NULL );
                nItemsAdded++;
            }
        }
        else
        {
            if (!m_bStartWidescreen)
            {
                itemID = m_pMode->AddItem( sz, NULL);
                nItemsAdded++;
            }
        }

        if (itemID == -1)
            continue;

        if ( plist->iWidth == m_CurrentSettings.w &&
             plist->iHeight == m_CurrentSettings.h )
        {
            selectedItemID = itemID;
        }

        const int resolutionDistance =
            std::abs(plist->iWidth - lastSelectedResolution.iWidth) +
            std::abs(plist->iHeight - lastSelectedResolution.iHeight);
        if (resolutionDistance < minimumResolutionDistance)
        {
            minimumResolutionDistance = resolutionDistance;
            nearestItemID = itemID;
        }
    }

    m_pAspectRatio->SetEnabled(foundWidescreen && foundNormal);

    const bool oppositeAspectAvailable = m_bStartWidescreen ? foundNormal : foundWidescreen;
    if (nItemsAdded == 0)
    {
        if (oppositeAspectAvailable)
        {
            m_bStartWidescreen = !m_bStartWidescreen;
            m_pAspectRatio->ActivateItem((m_pAspectRatio->GetActiveItem() + 1) % 2);
            PrepareResolutionList();
        }
        return;
    }

    if ( selectedItemID != -1 )
    {
        m_pMode->ActivateItem( selectedItemID );
    }
    else if (nearestItemID != -1)
    {
        m_pMode->ActivateItem( nearestItemID );
    }
    else
    {
        m_pMode->ActivateItem( 0 );
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::~COptionsSubVideo()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnResetData()
{
    // reset data
    RevertVidSettings();

    // reset UI elements
    m_pBrightnessSlider->Reset();
    m_pGammaSlider->Reset();
    m_pWindowed->SetSelected(m_CurrentSettings.windowed);
    m_pHDModels->SetSelected(m_CurrentSettings.hdmodels);
    m_pHighVideoQuality->SetSelected(m_CurrentSettings.vid_level > 0);
    m_pDisableMultitexture->SetSelected(m_CurrentSettings.disable_multitexture);
    m_pStretchAspect->SetSelected(m_CurrentSettings.stretch_aspect);
    m_pVsync->Reset();

    SetCurrentResolutionComboItem();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentResolutionComboItem()
{
    vmode_t *plist = NULL;
    int count = 0;
    g_pGameUIFuncs->GetVideoModes( &plist, &count );

    int resolution = -1;
    for ( int i = 0; i < count; i++, plist++ )
    {
        if ( plist->iWidth == m_CurrentSettings.w &&
             plist->iHeight == m_CurrentSettings.h )
        {
            resolution = i;
            break;
        }
    }

    if (resolution != -1)
    {
        char sz[256];
        sprintf(sz, "%i x %i", plist->iWidth, plist->iHeight);
        m_pMode->SetText(sz);
    }

}

bool COptionsSubVideo::GetSelectedResolution(int& width, int& height)
{
    if (!m_pMode || m_pMode->GetActiveItem() < 0)
        return false;

    char selectedResolution[256]{};
    m_pMode->GetItemText(m_pMode->GetActiveItem(), selectedResolution, sizeof(selectedResolution));

    int selectedWidth = 0;
    int selectedHeight = 0;
    char trailingCharacter = '\0';
    if (sscanf(selectedResolution, "%d x %d %c", &selectedWidth, &selectedHeight, &trailingCharacter) != 2)
        return false;

    vmode_t* modes = nullptr;
    int modeCount = 0;
    g_pGameUIFuncs->GetVideoModes(&modes, &modeCount);

    for (int i = 0; i < modeCount; ++i)
    {
        if (modes[i].iWidth == selectedWidth && modes[i].iHeight == selectedHeight)
        {
            width = selectedWidth;
            height = selectedHeight;
            return true;
        }
    }

    return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnApplyChanges()
{
    // Brightness, gamma and VSync are live cvars. Restarting the engine for
    // these controls discards the just-applied values and needlessly reloads
    // the whole graphics subsystem.
    m_pBrightnessSlider->ApplyChanges();
    m_pGammaSlider->ApplyChanges();
    m_pVsync->ApplyChanges();

    ApplyVidSettings();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::GetVidSettings()
{
    // Get original settings
    CVidSettings *p = &m_OrigSettings;

    g_pGameUIFuncs->GetCurrentVideoMode( &p->w, &p->h, &p->bpp );
    g_pGameUIFuncs->GetCurrentRenderer(p->renderer, 128, &p->windowed, &p->hdmodels, &p->addons_folder, &p->vid_level);
    p->disable_multitexture = m_pUserConfig->get_value_int("disable_multitexture", 0);
    p->stretch_aspect = m_pUserConfig->get_value_int("stretch_aspect", 0);

    strlwr( p->renderer );

    m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::RevertVidSettings()
{
    m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::ApplyVidSettings()
{
    // Retrieve text from active controls and parse out strings
    if ( m_pMode )
    {
        int selectedWidth = 0;
        int selectedHeight = 0;
        if (!GetSelectedResolution(selectedWidth, selectedHeight))
        {
            SetCurrentResolutionComboItem();
            return;
        }

        m_CurrentSettings.w = selectedWidth;
        m_CurrentSettings.h = selectedHeight;
    }

    if ( m_pWindowed )
    {
        bool checked = m_pWindowed->IsSelected();
        m_CurrentSettings.windowed = checked ? 1 : 0;
    }

    if ( m_pHDModels )
    {
        bool checked = m_pHDModels->IsSelected();
        m_CurrentSettings.hdmodels = checked ? 1 : 0;
    }

    if (m_pHighVideoQuality)
    {
        const bool checked = m_pHighVideoQuality->IsSelected();
        // GoldSrc stores 0 for low-detail mode and 1 for high-detail mode.
        m_CurrentSettings.vid_level = checked ? 1 : 0;
    }

    if ( m_pDisableMultitexture )
    {
        bool checked = m_pDisableMultitexture->IsSelected();
        m_CurrentSettings.disable_multitexture = checked ? 1 : 0;
    }

    if ( m_pStretchAspect )
    {
        bool checked = m_pStretchAspect->IsSelected();
        m_CurrentSettings.stretch_aspect = checked ? 1 : 0;
    }

    const bool videoModeChanged =
        m_OrigSettings.w != m_CurrentSettings.w ||
        m_OrigSettings.h != m_CurrentSettings.h ||
        m_OrigSettings.bpp != m_CurrentSettings.bpp;
    const bool windowModeChanged = m_OrigSettings.windowed != m_CurrentSettings.windowed;
    const bool hdModelsChanged = m_OrigSettings.hdmodels != m_CurrentSettings.hdmodels;
    const bool videoLevelChanged = m_OrigSettings.vid_level != m_CurrentSettings.vid_level;
    const bool multitextureChanged =
        m_OrigSettings.disable_multitexture != m_CurrentSettings.disable_multitexture;
    const bool stretchAspectChanged = m_OrigSettings.stretch_aspect != m_CurrentSettings.stretch_aspect;

    const bool restartRequired =
        videoModeChanged || windowModeChanged || hdModelsChanged || videoLevelChanged ||
        multitextureChanged || stretchAspectChanged;

    if (!restartRequired)
        return;

    CVidSettings *p = &m_CurrentSettings;

    char szCmd[ 256 ];

    if (videoModeChanged)
    {
        Q_snprintf(szCmd, sizeof(szCmd), "_setvideomode %i %i %i\n", p->w, p->h, p->bpp);
        engine->pfnClientCmd(szCmd);
    }

    if (windowModeChanged)
    {
        Q_snprintf(szCmd, sizeof(szCmd), "_setrenderer %s %s\n",
                   p->renderer, p->windowed ? "windowed" : "fullscreen");
        engine->pfnClientCmd(szCmd);
    }

    if (hdModelsChanged)
    {
        Q_snprintf(szCmd, sizeof(szCmd), "_sethdmodels %d\n", p->hdmodels);
        engine->pfnClientCmd(szCmd);
    }

    if (videoLevelChanged)
    {
        Q_snprintf(szCmd, sizeof(szCmd), "_set_vid_level %d\n", p->vid_level);
        engine->pfnClientCmd(szCmd);
    }

    if (multitextureChanged)
        m_pUserConfig->set_value("", "disable_multitexture", std::to_string(p->disable_multitexture), true);

    if (stretchAspectChanged)
        m_pUserConfig->set_value("", "stretch_aspect", std::to_string(p->stretch_aspect), true);

    // Prevent a second Apply before the queued restart from emitting the same
    // commands again.
    m_OrigSettings = m_CurrentSettings;

    // Force restart of entire engine
    engine->pfnClientCmd("fmod stop\n");
    engine->pfnClientCmd("_restart\n");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnButtonChecked(KeyValues *data)
{
    int state = data->GetInt("state");
    Panel* pPanel = (Panel*) data->GetPtr("panel", NULL);

    if (pPanel == m_pWindowed)
    {
        if (state != m_CurrentSettings.windowed)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pHDModels)
    {
        if (state != m_CurrentSettings.hdmodels)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pHighVideoQuality)
    {
        const int requestedVidLevel = state ? 1 : 0;
        if (requestedVidLevel != m_CurrentSettings.vid_level)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pDisableMultitexture)
    {
        if (state != m_CurrentSettings.disable_multitexture)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pStretchAspect)
    {
        if (state != m_CurrentSettings.stretch_aspect)
        {
            OnDataChanged();
        }
    }

    if (pPanel == m_pVsync)
    {
        OnDataChanged();
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnTextChanged(Panel *pPanel, const char *pszText)
{
    if (pPanel == m_pMode)
    {
        char sz[ 256 ];
        sprintf(sz, "%i x %i", m_CurrentSettings.w, m_CurrentSettings.h);

        if (strcmp(pszText, sz))
        {
            OnDataChanged();
        }
    }
    else if (pPanel == m_pAspectRatio )
    {
        if ( strcmp(pszText, m_pszAspectName[m_bStartWidescreen] ) )
        {
            m_bStartWidescreen = !m_bStartWidescreen;
            PrepareResolutionList();
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnDataChanged()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsSubVideo::OnCommand(const char *command)
{
    if (!stricmp(command, "Advanced"))
    {
        if (!m_hVideoAdvancedDialog.Get())
            m_hVideoAdvancedDialog = new CVideoAdvancedDialog(this);

        m_hVideoAdvancedDialog->Activate();
    }

    BaseClass::OnCommand(command);
}
