#pragma once
#include <string_view>

// GoldSrc starts a listen server by issuing "connect local". This is an
// engine loopback token, not a hostname and not an IP endpoint with a port.
inline bool IsEngineLocalConnectTarget(std::string_view target)
{
    constexpr std::string_view local = "local";
    if (target.size() != local.size()) return false;
    for (size_t i = 0; i < local.size(); ++i)
    {
        char ch = target[i];
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        if (ch != local[i]) return false;
    }
    return true;
}
