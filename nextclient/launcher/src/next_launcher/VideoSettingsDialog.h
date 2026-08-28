#pragma once

#include <Windows.h>

struct GameNetAccessStatus;

// Shows the launcher's restart-sensitive video settings page. Returns true
// only when the user explicitly chooses to launch the game.
bool ShowVideoSettingsDialog(HINSTANCE instance, const GameNetAccessStatus& access_status);
