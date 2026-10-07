#pragma once

#include <cstdint>
#include <steam/steamtypes.h>
#include <steam/isteamclient.h>

#pragma pack(push, 1)
// 768-byte (0x300) ticket format recognized by Reunion and DProto as SteamEmu (DP_AUTH_STEAMEMU).
// This avoids classification as AVSMP (which was triggered by 28-byte header_len=0x14)
// which Russian servers (like 46.174.52.26:27241) reject with "Sorry, AVSMP clients are not allowed on this server".
struct SteamEmuAuthTicket
{
    uint8_t  prefix[0x50];           // 0x00 - 0x4F: zeroes
    uint32_t magic;                  // 0x50 - 0x53: 0xFFFFFFFF (-1)
    uint32_t account_xor;            // 0x54 - 0x57: g_AccountID ^ 0xC9710266
    uint8_t  suffix[0x300 - 0x58];   // 0x58 - 0x2FF: zeroes
};
#pragma pack(pop)

static_assert(sizeof(SteamEmuAuthTicket) == 0x300, "SteamEmuAuthTicket must be exactly 768 (0x300) bytes");

namespace SteamEmu
{
    void Initialize();
    CSteamID GetSteamID();
    uint32_t GetAccountID();
    size_t GenerateAuthTicket(void* pDest, size_t maxLen, uint32_t unIPServer, uint16_t usPortServer);
}
