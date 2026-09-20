#pragma once

#include <string>

// Production grace period for offline LAN and subscription expiration.
inline constexpr bool kGameNetOneMinuteTest = false;
inline constexpr unsigned long long kGameNetGraceSeconds =
    kGameNetOneMinuteTest ? 60ULL : 10ULL * 86400ULL;

// Subscription list format: build tag | player name tag | expiry (YYYY/MM/DD).
// Blank lines and lines beginning with '#' are ignored.
inline constexpr wchar_t kGameNetAccessUrl[] = L"http://gameland.cam/client_tags.txt";

// Secret key for decrypting gameland_license.dat using RC4
inline constexpr char kLicenseSecretKey[] = "NextClientSecureRC4Key2026!";

// Gets the decrypted GameNetTag. Returns empty string if missing or invalid.
const std::string& GetGameNetTag();
