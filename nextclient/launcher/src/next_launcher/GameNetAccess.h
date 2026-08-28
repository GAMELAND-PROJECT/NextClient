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
    std::string expiry_date;

    [[nodiscard]] bool allowed() const { return state == GameNetAccessState::Active; }
};

// Performs one bounded HTTPS check during launcher startup.
// Failure is fail-closed for Online while the rest of the game remains usable.
GameNetAccessStatus QueryGameNetOnlineAccess();
