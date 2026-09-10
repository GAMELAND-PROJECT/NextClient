#pragma once
#include <cstdint>

inline constexpr std::uint32_t kOnlineMixServerMarker = 0x4D495858u; // "MIXX"
#include <string_view>

inline int CompareOnlinePopulation(int firstPlayers, int firstPing, std::string_view firstEndpoint,
                                   int secondPlayers, int secondPing, std::string_view secondEndpoint)
{
    if (firstPlayers != secondPlayers) return firstPlayers > secondPlayers ? -1 : 1;
    if (firstPing != secondPing) return firstPing < secondPing ? -1 : 1;
    return firstEndpoint.compare(secondEndpoint);
}
