#include "AutoUpdate.h"
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "wininet.lib")

#ifndef NEXTCLIENT_VERSION
#define NEXTCLIENT_VERSION "0.0.0"
#endif

#ifndef NEXTCLIENT_TAG
#define NEXTCLIENT_TAG "unknown"
#endif

namespace NextClient {

void AutoUpdate::CheckForUpdatesAndExitIfForced() {
    // All updates are strictly managed before launch via Allclient Launcher.
    // In-game execution is isolated and never interrupted.
    return;
}

}
