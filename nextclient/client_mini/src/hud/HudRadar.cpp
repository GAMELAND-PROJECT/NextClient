#include "HudRadar.h"
#include "../main.h"
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
    // rendered frame wastes CPU. Live radar positions still render every frame.
    constexpr float kPlayerInfoUpdateInterval = 0.25f;
    if (flTime < last_player_info_update_ || flTime - last_player_info_update_ >= kPlayerInfoUpdateInterval) {
        GetAllPlayersInfo();
        last_player_info_update_ = flTime;
    }

    if (isDemo)
    {
        extra_player_info_t* extraInfo = cl()->g_PlayerExtraInfo;
        if (extraInfo != nullptr)
        {
            // Update real-time origins and life status from interpolated entity state
            for (int i = 1; i <= MAX_PLAYERS; ++i)
            {
                cl_entity_t* ent = cl_enginefunc()->GetEntityByIndex(i);
                if (ent != nullptr && ent->player && ent->model != nullptr)
                {
                    // Copy exact 100 FPS interpolated entity origin
                    if (ent->origin[0] != 0.0f || ent->origin[1] != 0.0f || ent->origin[2] != 0.0f)
                    {
                        extraInfo[i].origin[0] = ent->origin[0];
                        extraInfo[i].origin[1] = ent->origin[1];
                        extraInfo[i].origin[2] = ent->origin[2];
                    }

                    // Synchronize life state so dead players don't remain as ghost dots
                    if (ent->curstate.solid == 0 || ent->curstate.health <= 0 || (ent->curstate.effects & 128))
                    {
                        extraInfo[i].dead = true;
                    }
                    else if (ent->curstate.health > 0)
                    {
                        extraInfo[i].dead = false;
                    }
                }
            }

            // Determine target player being watched in demo
            const int localIndex = (cl_enginefunc()->GetLocalPlayer() != nullptr) 
                                   ? cl_enginefunc()->GetLocalPlayer()->index 
                                   : 1;

            int targetPlayer = localIndex;
            if (g_iUser1 != 0 && g_iUser2 > 0 && g_iUser2 <= MAX_PLAYERS)
            {
                targetPlayer = g_iUser2;
            }

            int targetTeam = (targetPlayer > 0 && targetPlayer <= MAX_PLAYERS) 
                             ? extraInfo[targetPlayer].teamnumber 
                             : 0;

            // In free-roam spectator, default to active team (CT)
            if (targetTeam != 1 && targetTeam != 2)
            {
                targetTeam = 2;
            }

            const int originalLocalTeam = (localIndex > 0 && localIndex <= MAX_PLAYERS) 
                                          ? extraInfo[localIndex].teamnumber 
                                          : 0;

            // Align local team with spectated target's team so CHudHealth::DrawRadar renders all teammates of watched player
            if (localIndex > 0 && localIndex <= MAX_PLAYERS)
            {
                extraInfo[localIndex].teamnumber = targetTeam;
            }

            if ((!m_fPlayerDead || isDemo) && (m_bDrawRadar || isDemo))
                cl()->CHudHealth__DrawRadar(gHUD()->m_Health, flTime);

            // Restore original local team
            if (localIndex > 0 && localIndex <= MAX_PLAYERS)
            {
                extraInfo[localIndex].teamnumber = originalLocalTeam;
            }
            return;
        }
    }

    if ((!m_fPlayerDead || isDemo) && (m_bDrawRadar || isDemo))
        cl()->CHudHealth__DrawRadar(gHUD()->m_Health, flTime);
}
