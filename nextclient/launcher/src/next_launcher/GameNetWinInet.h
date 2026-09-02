#pragma once

#include <Windows.h>

#include <cstddef>
#include <string>

// Uses the current user's Internet Explorer/WinINet configuration. This is
// intentionally a desktop-only transport for older Windows installations.
bool DownloadWithWinInet(
    const wchar_t* url,
    size_t maximum_size,
    std::string& response,
    SYSTEMTIME* server_time = nullptr);
