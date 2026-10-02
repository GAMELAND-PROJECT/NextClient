#include "HudDeathNotice.h"
#include "../main.h"
#include "../utils.h"
#include <parsemsg.h>
#include "triangleapi.h"
#include <algorithm>

constexpr static auto KILL_RARITY_SPRITE = "sprites/kill_rarity.spr";
constexpr static int DEATHNOTICE_RIGHT = 16;

static int MsgFunc_DeathMsg(const char* pszName, int iSize, void* pbuf) {
	BEGIN_READ(pbuf, iSize);

	const int killer_id = READ_BYTE();
	const int victim_id = READ_BYTE();
	const bool is_headshot = READ_BYTE();
	std::string killed_with = READ_STRING();

	int assistant_id = 0;
	int kill_rarity_flags = 0;

	if(is_headshot)
		kill_rarity_flags |= KILLRARITY_HEADSHOT;
	
	auto extra_flags = (DeathMessageFlags)READ_LONG();
	if(READ_OK()) {
		if(extra_flags & PLAYERDEATH_POSITION) {
			READ_COORD();
			READ_COORD();
			READ_COORD();
		}

		if(extra_flags & PLAYERDEATH_ASSISTANT)
			assistant_id = READ_BYTE();

		if(extra_flags & PLAYERDEATH_KILLRARITY)
			kill_rarity_flags = READ_LONG();
	}

	auto hud = g_GameHud->get_deathnotice();
	HudDeathNotice::notice_row_t notice{};
	notice.killer_id = killer_id;
	notice.victim_id = victim_id;

	bool is_suicide = (killer_id == 0 || killer_id == victim_id);
	bool is_local_player_victim = false;

	if(hud->IsValidClientIndex(victim_id)) {
		hud_player_info_t victim_info;
		gEngfuncs.pfnGetPlayerInfo(victim_id, &victim_info);

		if(victim_info.name != nullptr)
			notice.victim_name = victim_info.name;
		
		notice.victim_color = hud->GetClientColor(victim_id);

		if(victim_info.thisplayer) {
			notice.is_should_dead_highlight = true;
			is_local_player_victim = true;
		}
	}

	if(is_suicide) {
		// In suicides, world damage, drowning, falling, or C4 bomb kills:
		// Do not show the player killing themselves ("Player [weapon] Player").
		// Keep killer and assistant empty so it renders purely as "[icon] Victim".
		notice.killer_name = "";
		notice.killer_color = nullptr;
		notice.assistant_name = "";
		notice.assistant_color = nullptr;
		notice.is_should_kill_highlight = false;

		if(killed_with.empty() || killed_with == "worldspawn" || killed_with == "world" || killed_with == "door" || killed_with == "trigger_hurt") {
			notice.weapon_sprite_index = hud->GetSkullSpriteIndex();
		} else {
			std::string wpn = "d_" + killed_with;
			int spriteIndex = gHUD->GetSpriteIndex(wpn.c_str());
			notice.weapon_sprite_index = spriteIndex != -1 ? spriteIndex : hud->GetSkullSpriteIndex();
		}
	} else {
		hud->HandleAmxxKillAssistCaseIfSo(killer_id, assistant_id, &notice);

		if(hud->IsValidClientIndex(killer_id)) {
			hud_player_info_t killer_info;
			gEngfuncs.pfnGetPlayerInfo(killer_id, &killer_info);

			if(notice.killer_name.empty() && killer_info.name != nullptr)
				notice.killer_name = killer_info.name;

			notice.killer_color = hud->GetClientColor(killer_id);

			if(killer_info.thisplayer) 
				notice.is_should_kill_highlight = true;
		}

		if(hud->IsValidClientIndex(assistant_id)) {
			hud_player_info_t assistant_info;
			gEngfuncs.pfnGetPlayerInfo(assistant_id, &assistant_info);

			if(assistant_info.name != nullptr)
				notice.assistant_name = assistant_info.name;

			notice.assistant_color = hud->GetClientColor(assistant_id);

			if(assistant_info.thisplayer) 
				notice.is_should_kill_highlight = true;
		}

		// Detect true teamkill
		bool is_tk = (killed_with == "teammate");
		if(!is_tk && hud->IsValidClientIndex(killer_id) && hud->IsValidClientIndex(victim_id)) {
			int k_team = hud->GetClientTeam(killer_id);
			int v_team = hud->GetClientTeam(victim_id);
			if(k_team != TEAM_UNASSIGNED && k_team == v_team) {
				is_tk = true;
			}
		}
		notice.is_teamkill = is_tk;

		if(notice.is_teamkill && notice.is_should_kill_highlight) {
			notice.is_should_kill_highlight = false;
		}

		std::string wpn = "d_" + killed_with;
		int spriteIndex = gHUD->GetSpriteIndex(wpn.c_str());
		notice.weapon_sprite_index = spriteIndex != -1 ? spriteIndex : hud->GetSkullSpriteIndex();
	}

	notice.display_time = *gHUD->m_flTime + hud->GetNoticeDisplayTime();

	if(kill_rarity_flags & KILLRARITY_DOMINATION) {
		if(is_local_player_victim && ~kill_rarity_flags & KILLRARITY_DOMINATION_BEGAN)
			kill_rarity_flags &= ~KILLRARITY_DOMINATION;
	}
	notice.kill_rarity_flags = (KillRarity)kill_rarity_flags;

	hud->PushDeathNotice(std::move(notice));

	return 0;
}

static int MsgFunc_DeathMsgWpnIcon(const char* pszName, int iSize, void* pbuf) {
	BEGIN_READ(pbuf, iSize);

	auto hud = g_GameHud->get_deathnotice();

	const char* sprite_path = READ_STRING();
	const int frame = READ_BYTE();
	const int rendermode = READ_BYTE();
	const int r = READ_BYTE();
	const int g = READ_BYTE();
	const int b = READ_BYTE();
	const int a = READ_BYTE();

    if (sprite_path == nullptr || !sprite_path[0] || !IsSafeSpriteFilePath(sprite_path)) {
        gEngfuncs.Con_DPrintf("MsgFunc_DeathMsgWpnIcon: invalid spritePath\n");
        return 1;
    }

	HudDeathNotice::wpn_icon_override_t wpn_icon{};
	wpn_icon.sprite = gEngfuncs.pfnSPR_Load(sprite_path);
	wpn_icon.frame = frame;
	wpn_icon.rendermode = rendermode;
	wpn_icon.color[0] = r / 255.0f;
	wpn_icon.color[1] = g / 255.0f;
	wpn_icon.color[2] = b / 255.0f;
	wpn_icon.alpha = a / 255.0f;

	float spr_w = gEngfuncs.pfnSPR_Width(wpn_icon.sprite, wpn_icon.frame);
	float spr_h = gEngfuncs.pfnSPR_Height(wpn_icon.sprite, wpn_icon.frame); 

	wpn_icon.ideal_scale = hud->GetDrawStringFontHeight() / spr_h;
	wpn_icon.ideal_w = std::ceil(spr_w * wpn_icon.ideal_scale);
	wpn_icon.ideal_h = std::ceil(spr_h * wpn_icon.ideal_scale);

	hud->SetWpnIconForNextMessage(std::move(wpn_icon));

	return 1;
}

void HudDeathNotice::SVC_UpdateUserInfo() {
	int id = eng()->MSG_ReadByte();
	int userId = eng()->MSG_ReadLong();

	auto userinfo = eng()->MSG_ReadString();
	auto current_name = client_state()->players[id].name;
	auto incoming_name = pmove->PM_Info_ValueForKey(userinfo, "name");

	if(current_name[0] && std::string(current_name) != incoming_name) {
		last_player_name_[id + 1] = { current_name, m_flTime + 1.5f };
	}
}

float calculateMatchingPercentage(const std::string& old_name, const std::string& new_name, size_t& mismatchPosition) {
    size_t m = 0;
    mismatchPosition = std::string::npos;

    for (m = 0; m < old_name.size(); m++) {
		if(m >= new_name.size() || old_name[m] != new_name[m])
			break;
    }

	while(m < new_name.size() && new_name[m] == '.')
		m++;

	mismatchPosition = m;

    return (static_cast<float>(m) / old_name.size()) * 100.0;
}

bool HudDeathNotice::HandleAmxxKillAssistCaseIfSo(int killer_id, int& assistant_id, HudDeathNotice::notice_row_t* notice) {
	auto it = last_player_name_.find(killer_id);
	if(it == last_player_name_.end()) return false;

	// Invalidate if cache expired (more than 1.5s old, meaning it wasn't a deathmsg assist rename)
	if(m_flTime > it->second.expire_time) {
		last_player_name_.erase(it);
		return false;
	}

	std::string old_name = it->second.name;

	if(assistant_id != 0) {
		notice->killer_name = old_name;
		last_player_name_.erase(it);
		return true;
	}

	hud_player_info_t player_info;
	cl_enginefunc()->pfnGetPlayerInfo(killer_id, &player_info);
	if(player_info.name == nullptr) return false;
	
	std::string new_name = player_info.name;

	constexpr const char* delim = " + ";
	constexpr size_t delim_len = std::string_view(delim).size();
	constexpr size_t min_name_len = delim_len + 2;

	if(new_name.length() < min_name_len || old_name.length() >= new_name.length()) return false;
	if(!new_name.contains(delim)) return false;

	size_t mismatch_pos;
	if(calculateMatchingPercentage(old_name, new_name, mismatch_pos) < 25.0) return false;

	size_t actual_delim_pos = mismatch_pos == std::string::npos ? old_name.length() : mismatch_pos;
	if(new_name.compare(actual_delim_pos, delim_len, delim, delim_len) != 0) return false;

	std::string dirty_assistant_name = new_name.substr(actual_delim_pos + delim_len);
	size_t first_dot_pos = dirty_assistant_name.find_last_not_of(".");
	if(first_dot_pos != std::string::npos && first_dot_pos != dirty_assistant_name.length() - 1)
		dirty_assistant_name.erase(first_dot_pos + 1);
 
	for(int i = 1; i < MAX_PLAYERS; i++) {
		hud_player_info_t a_info;
		cl_enginefunc()->pfnGetPlayerInfo(i, &a_info);

		if(a_info.name && std::string(a_info.name).starts_with(dirty_assistant_name)) {
			notice->killer_name = old_name;
			assistant_id = i;
			last_player_name_.erase(it);
			return true;
		}
	}
	return false;
}

HudDeathNotice::HudDeathNotice(nitroapi::NitroApiInterface* nitro_api)
	: HudBaseHelper(nitro_api) {

	DeferUnsub(cl()->CHudDeathNotice__Draw |= [this](CHudDeathNotice* const ptr, float flTime, const auto& next) {
		return cvar_deathnotice_old_->value ? next->Invoke(ptr, flTime) : 1;
	});

	DeferUnsub(cl()->UserMsg_DeathMsg += [this](const char* pszName, int iSize, void* pbuf_) {
		MsgFunc_DeathMsg(pszName, iSize, pbuf_);
	});

	DeferUnsub(eng()->SVC_UpdateUserInfo |= [this](const auto& next) {
		auto readcount = *eng()->msg_readcount;
		SVC_UpdateUserInfo();
		*eng()->msg_readcount = readcount;

		next->Invoke();
	});
}

void HudDeathNotice::PushDeathNotice(notice_row_t&& notice) {
	notice.custom_weapon_sprite = next_custom_weapon_sprite_;
	next_custom_weapon_sprite_ = {};

	notice_rows_.push_back(notice);
	if(notice_rows_.size() > cvar_deathnotice_max_->value) 
		notice_rows_.erase(notice_rows_.begin());
}

void HudDeathNotice::SetWpnIconForNextMessage(wpn_icon_override_t&& wpn_icon) {
	next_custom_weapon_sprite_ = wpn_icon;
}

void HudDeathNotice::Init() {
	cvar_deathnotice_time_ = cl_enginefunc()->pfnGetCvarPointer("hud_deathnotice_time");
	cvar_deathnotice_max_ = cl_enginefunc()->pfnRegisterVariable("hud_deathnotice_max", "5", FCVAR_ARCHIVE);
	cvar_deathnotice_old_ = cl_enginefunc()->pfnRegisterVariable("hud_deathnotice_old", "0", FCVAR_ARCHIVE);

	cl_enginefunc()->pfnHookUserMsg("DeathMsgWpn", MsgFunc_DeathMsgWpnIcon);
}

void HudDeathNotice::VidInit() {
	kill_rarity_sprite_ = LoadSprite(KILL_RARITY_SPRITE);

	skull_sprite_index_ = gHUD()->GetSpriteIndex("d_skull");
	draw_string_font_height_ = DrawConsoleStringHeight();

	kill_rarity_sprite_scale_ = 0.375f;
	kill_rarity_sprite_width_ = SPR_Width(kill_rarity_sprite_, 0) * kill_rarity_sprite_scale_;
	kill_rarity_sprite_height_ = SPR_Height(kill_rarity_sprite_, 0) * kill_rarity_sprite_scale_;
	kill_rarity_sprite_alpha_ = 0.85f;
	kill_rarity_sprite_rendermode_ = kRenderTransAdd;
	kill_rarity_sprite_padding_x_ = 3;

	weapon_sprite_padding_x_ = 3;
	string_padding_x_ = 3;

	notice_boxes_gap_ = 4;
	notice_box_padding_top_ = 3;
	notice_box_padding_bottom_ = 3;
	notice_box_outline_width_ = 1;
	notice_box_padding_x_ = 7;
	notice_box_height_ = draw_string_font_height_ + notice_box_padding_top_ + notice_box_padding_bottom_;

	notice_rows_.clear();
}

int HudDeathNotice::DrawScaledSprite(
	HSPRITE_t* sprite, int frame,
	int x, int y, float scale,
	int rendermode, vec3_t color, float alpha
) {
    const auto sprite_ptr = gEngfuncs.GetSpritePointer(*sprite);
	if(sprite_ptr == nullptr) return x;
	
    const auto tri = gEngfuncs.pTriAPI;

	tri->SpriteTexture(const_cast<model_s*>(sprite_ptr), frame);
    tri->RenderMode(rendermode);
    tri->Color4f(color[0], color[1], color[2], alpha);
    tri->CullFace(TRI_NONE);

	int w = gEngfuncs.pfnSPR_Width(*sprite, frame) * scale;
	int h = gEngfuncs.pfnSPR_Height(*sprite, frame) * scale;

	tri->Begin(TRI_QUADS);
	tri->TexCoord2f(0, 1);
	tri->Vertex3f(x, y + h, 0);
	tri->TexCoord2f(1, 1);
	tri->Vertex3f(x + w, y + h, 0);
	tri->TexCoord2f(1, 0);
	tri->Vertex3f(x + w, y, 0);
	tri->TexCoord2f(0, 0);
	tri->Vertex3f(x, y, 0);
	tri->End();

	tri->RenderMode(kRenderNormal);

	return x + w;
}

int HudDeathNotice::DrawKillRaritySprite(RarityFrame type, int x, int y, float alpha) {
	return DrawScaledSprite(
		&kill_rarity_sprite_, type,
		x + kill_rarity_sprite_padding_x_, y, kill_rarity_sprite_scale_, 
		kill_rarity_sprite_rendermode_, sprite_icons_color_, kill_rarity_sprite_alpha_ * alpha
	) + kill_rarity_sprite_padding_x_;
}

int HudDeathNotice::GetKillRaritySpriteFullWidth(int frame) {
	int spr_w = SPR_Width(kill_rarity_sprite_, frame) * kill_rarity_sprite_scale_;
	return spr_w + (kill_rarity_sprite_padding_x_ * 2);
}

int HudDeathNotice::DrawWeaponSprite(int index, int x, int y, float alpha) {
	if(index == -1) index = skull_sprite_index_;
	if(index == -1) return x;

	byte c = (byte)std::clamp((int)(255.0f * alpha + 0.5f), 0, 255);
	SPR_Set(gHUD()->GetSprite(index), c, c, c);
	x += weapon_sprite_padding_x_;
	SPR_DrawAdditive(0, x, y, &gHUD()->GetSpriteRect(index));
	return x + gHUD()->GetSpriteWidth(index) + weapon_sprite_padding_x_;
}

int HudDeathNotice::GetWeaponSpriteFullWidth(int index) {
	if(index == -1) index = skull_sprite_index_;
	if(index == -1) return weapon_sprite_padding_x_ * 2;
	return gHUD()->GetSpriteWidth(index) + weapon_sprite_padding_x_ * 2;
}

int HudDeathNotice::DrawString(const char* text, vec3_t color, int x, int y, float alpha) {
	if(!text || !*text) return x;
	vec3_t faded_color = {
		std::clamp(color[0] * alpha, 0.0f, 1.0f),
		std::clamp(color[1] * alpha, 0.0f, 1.0f),
		std::clamp(color[2] * alpha, 0.0f, 1.0f)
	};
	DrawSetTextColor(faded_color);
	return DrawConsoleString(text, x + string_padding_x_, y) + string_padding_x_;
}

int HudDeathNotice::GetStringFullWidth(const char* text) {
	if(!text || !*text) return 0;
	return DrawConsoleStringLen(text) + string_padding_x_ * 2;
}

int HudDeathNotice::GetCustomWeaponSpriteFullWidth(wpn_icon_override_t* icon) {
	return icon->ideal_w + weapon_sprite_padding_x_ * 2;
}

int HudDeathNotice::GetCustomWeaponSpriteHeight(wpn_icon_override_t* icon) {
	return icon->ideal_h;
}

int HudDeathNotice::DrawCustomWeaponSprite(wpn_icon_override_t* icon, int x, int y, float alpha) {
	return DrawScaledSprite(
		&icon->sprite, icon->frame,
		x + weapon_sprite_padding_x_, y, icon->ideal_scale, 
		icon->rendermode, icon->color, icon->alpha * alpha
	) + weapon_sprite_padding_x_;
}

void HudDeathNotice::Draw(float flTime) {
	if(cvar_deathnotice_old_->value) return;

	int screen_w, screen_h;
	GetScreenResolution(screen_w, screen_h);

	int i = 0;
	for(auto notice = notice_rows_.begin(); notice != notice_rows_.end(); ) {
		if(notice->display_time < flTime) {
			notice = notice_rows_.erase(notice);
			continue;
		}

		notice->display_time = std::min(notice->display_time, m_flTime + cvar_deathnotice_time_->value);

		// Calculate smooth fade-out alpha during the last 1.2 seconds
		float time_remaining = notice->display_time - flTime;
		float fade_alpha = (time_remaining < 1.2f) ? std::clamp(time_remaining / 1.2f, 0.0f, 1.0f) : 1.0f;

		// Clean modern esports top anchor (not pushed 90px down on 1080p/1440p)
		int top_offset = std::clamp((int)(20.0f * (screen_h / 720.0f) + 0.5f), 18, 34);
		if(g_iUser1 != 0)
			top_offset += 64; // Spectator top banner clearance

		int row_step = notice_box_height_ + (notice_box_outline_width_ * 2) + notice_boxes_gap_;
		int y = top_offset + (row_step * i);

		// Forward content width calculation
		int content_w = 0;

		bool has_domination = (notice->kill_rarity_flags & KILLRARITY_DOMINATION);
		bool has_revenge = (!has_domination && (notice->kill_rarity_flags & KILLRARITY_REVENGE));
		if(has_domination) content_w += GetKillRaritySpriteFullWidth(RarityFrame::DOMINATION);
		else if(has_revenge) content_w += GetKillRaritySpriteFullWidth(RarityFrame::REVENGE);

		if(notice->kill_rarity_flags & KILLRARITY_KILLER_BLIND)
			content_w += GetKillRaritySpriteFullWidth(RarityFrame::KILLER_BLIND);

		if(!notice->killer_name.empty())
			content_w += GetStringFullWidth(notice->killer_name.c_str());

		bool has_assist = !notice->assistant_name.empty();
		bool has_assist_flash = (has_assist && (notice->kill_rarity_flags & KILLRARITY_ASSISTEDFLASH));
		if(has_assist) {
			content_w += GetStringFullWidth("+");
			if(has_assist_flash)
				content_w += GetKillRaritySpriteFullWidth(RarityFrame::ASSIST_FLASH);
			content_w += GetStringFullWidth(notice->assistant_name.c_str());
		}

		if(notice->kill_rarity_flags & KILLRARITY_INAIR)
			content_w += GetKillRaritySpriteFullWidth(RarityFrame::KILLER_INAIR);

		int weapon_sprite_full_w = 0;
		int weapon_sprite_h = 0;
		if(notice->custom_weapon_sprite.sprite) {
			weapon_sprite_full_w = GetCustomWeaponSpriteFullWidth(&notice->custom_weapon_sprite);
			weapon_sprite_h = GetCustomWeaponSpriteHeight(&notice->custom_weapon_sprite);
		} else {
			weapon_sprite_full_w = GetWeaponSpriteFullWidth(notice->weapon_sprite_index);
			weapon_sprite_h = (notice->weapon_sprite_index != -1) ? gHUD()->GetSpriteHeight(notice->weapon_sprite_index) : 16;
		}
		content_w += weapon_sprite_full_w;

		if(notice->kill_rarity_flags & KILLRARITY_NOSCOPE)
			content_w += GetKillRaritySpriteFullWidth(RarityFrame::NOSCOPE);
		if(notice->kill_rarity_flags & KILLRARITY_THRUSMOKE)
			content_w += GetKillRaritySpriteFullWidth(RarityFrame::THROUGH_SMOKE);
		if(notice->kill_rarity_flags & KILLRARITY_PENETRATED)
			content_w += GetKillRaritySpriteFullWidth(RarityFrame::PENETRATED);
		if(notice->kill_rarity_flags & KILLRARITY_HEADSHOT)
			content_w += GetKillRaritySpriteFullWidth(RarityFrame::HEADSHOT);

		if(!notice->victim_name.empty())
			content_w += GetStringFullWidth(notice->victim_name.c_str());

		int total_card_w = content_w + (notice_box_padding_x_ * 2);
		int card_right = screen_w - DEATHNOTICE_RIGHT;
		int card_left = card_right - total_card_w;

		// Card background & outline with smooth fade-out
		if(notice->is_should_dead_highlight) {
			// Local player died: Crimson/burgundy with red border
			DrawOutlinedRect(
				card_left, y, card_right, y + notice_box_height_,
				125, 15, 25, (byte)(160 * fade_alpha),
				notice_box_outline_width_,
				200, 35, 35, (byte)(210 * fade_alpha)
			);
		}
		else if(notice->is_should_kill_highlight) {
			// Local player kill: Sleek esports dark card with glowing golden/amber border
			DrawOutlinedRect(
				card_left, y, card_right, y + notice_box_height_,
				16, 18, 22, (byte)(180 * fade_alpha),
				notice_box_outline_width_,
				255, 185, 20, (byte)(255 * fade_alpha)
			);
		}
		else if(notice->is_teamkill) {
			// Teamkill: Dark card with warning orange outline
			DrawOutlinedRect(
				card_left, y, card_right, y + notice_box_height_,
				25, 15, 10, (byte)(160 * fade_alpha),
				notice_box_outline_width_,
				230, 90, 10, (byte)(200 * fade_alpha)
			);
		}
		else {
			// Regular kill: Translucent dark card with subtle charcoal border
			DrawOutlinedRect(
				card_left, y, card_right, y + notice_box_height_,
				12, 14, 18, (byte)(140 * fade_alpha),
				notice_box_outline_width_,
				45, 50, 60, (byte)(130 * fade_alpha)
			);
		}

		int weapon_sprite_optimal_y = y + ((notice_box_height_ - weapon_sprite_h) / 2);
		int kill_rarity_sprite_optimal_y = y + ((notice_box_height_ - kill_rarity_sprite_height_) / 2);
		int draw_string_optimal_y = y + notice_box_padding_top_;

		int cur_x = card_left + notice_box_padding_x_;

		if(has_domination)
			cur_x = DrawKillRaritySprite(RarityFrame::DOMINATION, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);
		else if(has_revenge)
			cur_x = DrawKillRaritySprite(RarityFrame::REVENGE, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(notice->kill_rarity_flags & KILLRARITY_KILLER_BLIND)
			cur_x = DrawKillRaritySprite(RarityFrame::KILLER_BLIND, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(!notice->killer_name.empty()) {
			float* k_color = notice->killer_color ? notice->killer_color : sprite_icons_color_;
			cur_x = DrawString(notice->killer_name.c_str(), k_color, cur_x, draw_string_optimal_y, fade_alpha);
		}

		if(has_assist) {
			cur_x = DrawString("+", sprite_icons_color_, cur_x, draw_string_optimal_y, fade_alpha);

			if(has_assist_flash)
				cur_x = DrawKillRaritySprite(RarityFrame::ASSIST_FLASH, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

			float* a_color = notice->assistant_color ? notice->assistant_color : sprite_icons_color_;
			cur_x = DrawString(notice->assistant_name.c_str(), a_color, cur_x, draw_string_optimal_y, fade_alpha);
		}

		// In-air icon: vertically centered without unnatural negative offset!
		if(notice->kill_rarity_flags & KILLRARITY_INAIR)
			cur_x = DrawKillRaritySprite(RarityFrame::KILLER_INAIR, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(notice->custom_weapon_sprite.sprite)
			cur_x = DrawCustomWeaponSprite(&notice->custom_weapon_sprite, cur_x, weapon_sprite_optimal_y, fade_alpha);
		else
			cur_x = DrawWeaponSprite(notice->weapon_sprite_index, cur_x, weapon_sprite_optimal_y, fade_alpha);

		if(notice->kill_rarity_flags & KILLRARITY_NOSCOPE)
			cur_x = DrawKillRaritySprite(RarityFrame::NOSCOPE, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(notice->kill_rarity_flags & KILLRARITY_THRUSMOKE)
			cur_x = DrawKillRaritySprite(RarityFrame::THROUGH_SMOKE, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(notice->kill_rarity_flags & KILLRARITY_PENETRATED)
			cur_x = DrawKillRaritySprite(RarityFrame::PENETRATED, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(notice->kill_rarity_flags & KILLRARITY_HEADSHOT)
			cur_x = DrawKillRaritySprite(RarityFrame::HEADSHOT, cur_x, kill_rarity_sprite_optimal_y, fade_alpha);

		if(!notice->victim_name.empty()) {
			float* v_color = notice->victim_color ? notice->victim_color : sprite_icons_color_;
			cur_x = DrawString(notice->victim_name.c_str(), v_color, cur_x, draw_string_optimal_y, fade_alpha);
		}

		i++;
		notice++;
	}
}
