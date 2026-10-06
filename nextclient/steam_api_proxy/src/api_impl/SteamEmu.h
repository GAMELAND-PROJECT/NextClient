#pragma once

#include <cstdint>
#include <steam/steamtypes.h>
#include <steam/isteamclient.h>

#pragma pack(push, 1)
struct SteamEmuAuthTicket
{
    uint32_t magic;          // 0x00: 0x554D4548 ("HEMU")
    uint32_t version;        // 0x04: 0x0000013B (315)
    uint32_t server_ip;      // 0x08: htonl(unIPServer)
    uint32_t client_ip;      // 0x0C: 0
    uint32_t steam_id_low;   // 0x10: low 32 bits (account_id)
    uint32_t steam_id_high;  // 0x14: high 32 bits (0x01100001)
    uint32_t app_id;         // 0x18: 10 (CS 1.6 AppID)
    uint32_t flags;          // 0x1C: 0
    uint32_t unk1;           // 0x20: 0
    uint32_t unk2;           // 0x24: 0
    uint8_t  unk3;           // 0x28: 0
    uint8_t  pad[7];         // 0x29 - 0x2F: 0
    uint32_t steam_id_low2;  // 0x30: low 32 bits
    uint32_t steam_id_high2; // 0x34: high 32 bits
    uint32_t inner_size;     // 0x38: 0x20 (32)
    
    // Inner block at 0x3C:
    uint32_t inner_magic1;   // 0x3C: 0
    uint32_t inner_magic2;   // 0x40: 0x20455353 ("SSE ")
    uint32_t inner_sid_low;  // 0x44: low 32 bits
    uint32_t inner_sid_high; // 0x48: high 32 bits
    uint32_t inner_unk1;     // 0x4C: 0
    uint32_t inner_unk2;     // 0x50: 0
    uint32_t inner_unk3;     // 0x54: 0
    uint32_t inner_unk4;     // 0x58: 0
    uint32_t inner_srv_ip;   // 0x5C: htonl(unIPServer)
    uint16_t inner_srv_port; // 0x60: htons(usPortServer)
    uint16_t inner_pad;      // 0x62: 0
    uint32_t inner_unk5;     // 0x64: 0
    uint32_t inner_checksum; // 0x68: 0
};
#pragma pack(pop)

static_assert(sizeof(SteamEmuAuthTicket) == 108, "SteamEmuAuthTicket must be exactly 108 bytes");

namespace SteamEmu
{
    void Initialize();
    CSteamID GetSteamID();
    uint32_t GetAccountID();
    size_t GenerateAuthTicket(void* pDest, size_t maxLen, uint32_t unIPServer, uint16_t usPortServer);
}
