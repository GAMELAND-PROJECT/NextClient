#pragma once

#include <cstdint>
#include <steam/steamtypes.h>
#include <steam/isteamclient.h>

#pragma pack(push, 1)
// 28-byte ticket recognized by Reunion and DProto (SC2009 / AVSMP)
struct SteamEmuAuthTicket
{
    uint32_t header_len;     // 0x00: 20 (0x14)
    uint32_t unk1;           // 0x04: 0
    uint32_t unk2;           // 0x08: 0
    uint32_t account_id;     // 0x0C: AccountID (1792139526 -> STEAM_0:0:896069763)
    uint32_t unk3;           // 0x10: 0
    uint32_t unk4;           // 0x14: 0
    uint32_t unk5;           // 0x18: 0
};
#pragma pack(pop)

static_assert(sizeof(SteamEmuAuthTicket) == 28, "SteamEmuAuthTicket must be exactly 28 bytes");

namespace SteamEmu
{
    void Initialize();
    CSteamID GetSteamID();
    uint32_t GetAccountID();
    size_t GenerateAuthTicket(void* pDest, size_t maxLen, uint32_t unIPServer, uint16_t usPortServer);
}
