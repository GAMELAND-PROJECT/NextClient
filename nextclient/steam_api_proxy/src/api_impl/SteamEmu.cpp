#include "SteamEmu.h"
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace SteamEmu
{
    static inline uint32_t HostToNet32(uint32_t val)
    {
        return ((val & 0x000000FF) << 24) |
               ((val & 0x0000FF00) << 8)  |
               ((val & 0x00FF0000) >> 8)  |
               ((val & 0xFF000000) >> 24);
    }

    static inline uint16_t HostToNet16(uint16_t val)
    {
        return (uint16_t)(((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8));
    }
    static uint32_t g_AccountID = 0;
    static CSteamID g_SteamID;
    static bool g_Initialized = false;

    void Initialize()
    {
        if (g_Initialized && g_AccountID != 0)
            return;

        g_Initialized = true;

        // 1. Try reading existing persistent ID from disk
        const char* candidatePaths[] = {
            "platform\\steam\\steam_autogen_id.dat",
            "steam_autogen_id.dat",
            "cstrike\\steam_autogen_id.dat"
        };

        for (const char* path : candidatePaths)
        {
            FILE* f = nullptr;
#ifdef _WIN32
            if (fopen_s(&f, path, "rb") == 0 && f)
#else
            f = fopen(path, "rb");
            if (f)
#endif
            {
                uint32_t saved = 0;
                if (fread(&saved, sizeof(saved), 1, f) == 1 && saved >= 10000000 && saved <= 0x7FFFFFFF)
                {
                    fclose(f);
                    g_AccountID = saved;
                    g_SteamID = CSteamID(g_AccountID, k_EUniversePublic, k_EAccountTypeIndividual);
                    return;
                }
                fclose(f);
            }
        }

        // 2. Hardware Fingerprinting (Unique per PC in GameNet)
#ifdef _WIN32
        DWORD volSerial = 0;
        GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);

        char machineGuid[128] = { 0 };
        HKEY hKey = NULL;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
        {
            DWORD dwType = REG_SZ;
            DWORD dwSize = sizeof(machineGuid) - 1;
            RegQueryValueExA(hKey, "MachineGuid", NULL, &dwType, (LPBYTE)machineGuid, &dwSize);
            RegCloseKey(hKey);
        }

        char computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD compSize = sizeof(computerName);
        GetComputerNameA(computerName, &compSize);

        // FNV-1a 64-bit hash
        uint64_t hash = 14695981039346656037ULL;
        auto mix = [&hash](const void* data, size_t len) {
            const uint8_t* p = (const uint8_t*)data;
            for (size_t i = 0; i < len; ++i) {
                hash ^= p[i];
                hash *= 1099511628211ULL;
            }
        };

        mix(&volSerial, sizeof(volSerial));
        if (machineGuid[0])
            mix(machineGuid, strlen(machineGuid));
        if (computerName[0])
            mix(computerName, strlen(computerName));

        uint32_t accId = (uint32_t)(hash ^ (hash >> 32));
#else
        uint32_t accId = 12345678;
#endif

        accId &= 0x7FFFFFFF;
        if (accId < 10000000)
            accId += 10000000;

        g_AccountID = accId;
        g_SteamID = CSteamID(g_AccountID, k_EUniversePublic, k_EAccountTypeIndividual);

        // 3. Persist to disk so identity remains permanent across sessions
        FILE* f = nullptr;
#ifdef _WIN32
        if (fopen_s(&f, "platform\\steam\\steam_autogen_id.dat", "wb") == 0 && f)
        {
            fwrite(&g_AccountID, sizeof(g_AccountID), 1, f);
            fclose(f);
        }
        else if (fopen_s(&f, "steam_autogen_id.dat", "wb") == 0 && f)
        {
            fwrite(&g_AccountID, sizeof(g_AccountID), 1, f);
            fclose(f);
        }
#endif
    }

    CSteamID GetSteamID()
    {
        if (!g_Initialized || g_AccountID == 0)
            Initialize();

        return g_SteamID;
    }

    uint32_t GetAccountID()
    {
        if (!g_Initialized || g_AccountID == 0)
            Initialize();

        return g_AccountID;
    }

    size_t GenerateAuthTicket(void* pDest, size_t maxLen, uint32_t unIPServer, uint16_t usPortServer)
    {
        if (!pDest || maxLen < sizeof(SteamEmuAuthTicket))
            return 0;

        if (!g_Initialized || g_AccountID == 0)
            Initialize();

        SteamEmuAuthTicket* t = reinterpret_cast<SteamEmuAuthTicket*>(pDest);
        memset(t, 0, sizeof(*t));

        // Header
        t->magic = 0x554D4548;           // "HEMU"
        t->version = 0x0000013B;         // Version 315
        t->server_ip = HostToNet32(unIPServer);
        t->client_ip = 0;
        t->steam_id_low = g_AccountID;
        t->steam_id_high = 0x01100001;    // UniversePublic | Individual | Instance1
        t->app_id = 10;                  // Counter-Strike 1.6 AppID
        t->flags = 0;
        t->steam_id_low2 = g_AccountID;
        t->steam_id_high2 = 0x01100001;
        t->inner_size = 0x20;            // 32 bytes

        // Inner Block at 0x3C (recognized by SmartSteamEmu and Reunion)
        t->inner_magic1 = 0;
        t->inner_magic2 = 0x20455353;    // "SSE "
        t->inner_sid_low = g_AccountID;
        t->inner_sid_high = 0x01100001;
        t->inner_srv_ip = HostToNet32(unIPServer);
        t->inner_srv_port = HostToNet16(usPortServer);

        return sizeof(SteamEmuAuthTicket);
    }
}
