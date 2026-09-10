#pragma once
#include <string_view>

inline bool IsOnlineMixCategory(std::string_view category)
{
    return category == "m" || category == "M";
}

inline int CompareOnlinePopulation(int firstPlayers, int firstPing, std::string_view firstEndpoint,
                                   int secondPlayers, int secondPing, std::string_view secondEndpoint)
{
    if (firstPlayers != secondPlayers) return firstPlayers > secondPlayers ? -1 : 1;
    if (firstPing != secondPing) return firstPing < secondPing ? -1 : 1;
    return firstEndpoint.compare(secondEndpoint);
}
