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

// Checks access at startup. LAN requires a successful verification in the last 15 days.
GameNetAccessStatus QueryGameNetOnlineAccess();

// Downloads the shared managed-server password once during launcher startup.
// The returned value is passed only to the in-process engine and is never logged.
std::string QueryGameNetServerPassword();
