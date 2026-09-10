#pragma once

#include <string>

enum class GameNetAccessState
{
    Active,
    Expired,
    TagMissing,
    InvalidEntry,
    ServiceUnavailable,
};

struct GameNetAccessStatus
{
    GameNetAccessState state = GameNetAccessState::ServiceUnavailable;
    std::string tag;
    std::string player_name_tag;
    std::string expiry_date;
    int days_remaining = -1;
    bool lan_allowed = false;
    int offline_days_remaining = 0;

    [[nodiscard]] bool allowed() const { return state == GameNetAccessState::Active; }
};

// Checks access at startup. LAN requires verification within the configured grace period.
GameNetAccessStatus QueryGameNetOnlineAccess();
