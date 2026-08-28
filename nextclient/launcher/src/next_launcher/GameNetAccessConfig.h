#pragma once

#ifndef NEXTCLIENT_GAME_NET_TAG
#define NEXTCLIENT_GAME_NET_TAG "im"
#endif

// CMake reads this value from the repository-root client_tags.txt file.
inline constexpr char kGameNetTag[] = NEXTCLIENT_GAME_NET_TAG;

// Subscription list format: build tag | player name tag | expiry (YYYY/MM/DD).
// Blank lines and lines beginning with '#' are ignored.
inline constexpr wchar_t kGameNetAccessUrl[] = L"https://gameland.cam/client_tags.txt";

// The response contains one password shared by every managed pinned server.
// HTTPS protects it in transit, but a public URL does not make it secret.
inline constexpr wchar_t kGameNetServerPasswordUrl[] =
    L"https://gameland.cam/server_password.txt";
