#include "HudRadar.h"
#include <demo_api.h>

HudRadar::HudRadar(nitroapi::NitroApiInterface* nitro_api) 
	: HudBaseHelper(nitro_api)
	, m_bDrawRadar(gHUD()->m_Health->m_bDrawRadar) {}

void HudRadar::Draw(float flTime) {
	if (m_iHideHUDDisplay & HIDEHUD_HEALTH)
		return;

    const bool isDemo = (cl_enginefunc()->pDemoAPI && cl_enginefunc()->pDemoAPI->IsPlayingback());
    if (!isDemo && cl_enginefunc()->IsSpectateOnly())
        return;

    // Names/teams change infrequently; refreshing all player records every
    // rendered frame wastes CPU. Radar positions are still drawn every frame.
    // Player names and teams are slow-changing metadata. Four refreshes per
    // second are enough; live radar positions still render every frame.
    constexpr float kPlayerInfoUpdateInterval = 0.25f;
    if (flTime < last_player_info_update_ || flTime - last_player_info_update_ >= kPlayerInfoUpdateInterval) {
        GetAllPlayersInfo();
        last_player_info_update_ = flTime;
    }

    if ((!m_fPlayerDead || isDemo) && (m_bDrawRadar || isDemo))
        cl()->CHudHealth__DrawRadar(gHUD()->m_Health, flTime);
}
