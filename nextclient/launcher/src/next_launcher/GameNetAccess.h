#pragma once

// Performs one bounded HTTPS check during launcher startup.
// Failure is fail-closed for Online while the rest of the game remains usable.
bool IsGameNetOnlineAccessAllowed();

