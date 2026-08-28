#pragma once

// Change only this value before building a package for another game net.
// Keep tags short, lowercase, and unique (for example: im, gl, px).
inline constexpr char kGameNetTag[] = "im";

// Plain-text subscription list. Format: tag | Jalali expiry (YYYY/MM/DD).
// Blank lines and lines beginning with '#' are ignored.
inline constexpr wchar_t kGameNetAccessUrl[] = L"https://gameland.cam/client_tags.txt";
