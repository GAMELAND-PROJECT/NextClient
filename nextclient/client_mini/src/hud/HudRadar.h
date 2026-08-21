#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudRadar : public HudBase, public HudBaseHelper {
	bool &m_bDrawRadar;
	float last_player_info_update_ = -1.0f;
	
public:
	explicit HudRadar(nitroapi::NitroApiInterface* nitro_api);

	void Draw(float flTime) override;
};
