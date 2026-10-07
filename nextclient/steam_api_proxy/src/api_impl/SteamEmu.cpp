#include "SteamEmu.h"
#include <cstdio>
#include <cstdlib>
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
                    // Discard the buggy hardcoded ID so affected clients get a fresh unique ID
                    if (saved != 1792139526)
                    {
                        g_AccountID = saved;
                        g_SteamID = CSteamID(g_AccountID, k_EUniversePublic, k_EAccountTypeIndividual);
                        return;
                    }
                }
                else
                {
                    fclose(f);
                }
            }
        }

        // 2. Hardware Fingerprinting (Unique per PC)
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

        char installId[128] = { 0 };
        hKey = NULL;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\NextClient", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
            RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\NextClient", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            DWORD dwType = REG_SZ;
            DWORD dwSize = sizeof(installId) - 1;
            RegQueryValueExA(hKey, "InstallID", NULL, &dwType, (LPBYTE)installId, &dwSize);
            RegCloseKey(hKey);
        }

        char computerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD compSize = sizeof(computerName);
        GetComputerNameA(computerName, &compSize);

        char userName[256] = { 0 };
        DWORD userSize = sizeof(userName);
        GetUserNameA(userName, &userSize);

        // FNV-1a 64-bit hash
        uint64_t hash = 14695981039346656037ULL;
        auto mix = [&hash](const void* data, size_t len) {
            const uint8_t* p = (const uint8_t*)data;
            for (size_t i = 0; i < len; ++i) {
                hash ^= p[i];
                hash *= 1099511628211ULL;
            }
        };

        if (volSerial != 0)
            mix(&volSerial, sizeof(volSerial));
        if (machineGuid[0])
            mix(machineGuid, strlen(machineGuid));
        if (installId[0])
            mix(installId, strlen(installId));
        if (computerName[0])
            mix(computerName, strlen(computerName));
        if (userName[0])
            mix(userName, strlen(userName));

        // Network adapters (MAC address)
        HMODULE hIphlp = LoadLibraryA("iphlpapi.dll");
        if (hIphlp)
        {
            typedef DWORD (WINAPI *GetAdaptersInfo_t)(void*, PULONG);
            auto pGetAdaptersInfo = (GetAdaptersInfo_t)GetProcAddress(hIphlp, "GetAdaptersInfo");
            if (pGetAdaptersInfo)
            {
                ULONG outBufLen = 0;
                DWORD ret = pGetAdaptersInfo(nullptr, &outBufLen);
                if ((ret == ERROR_BUFFER_OVERFLOW || ret == 111) && outBufLen > 0 && outBufLen < 65536)
                {
                    void* pBuf = malloc(outBufLen);
                    if (pBuf)
                    {
                        if (pGetAdaptersInfo(pBuf, &outBufLen) == ERROR_SUCCESS)
                        {
                            mix(pBuf, outBufLen);
                        }
                        free(pBuf);
                    }
                }
            }
            FreeLibrary(hIphlp);
        }

        // If hash was somehow unaffected, add high-resolution counter entropy
        if (hash == 14695981039346656037ULL)
        {
            LARGE_INTEGER qpc;
            QueryPerformanceCounter(&qpc);
            mix(&qpc.QuadPart, sizeof(qpc.QuadPart));
        }

        uint32_t accId = (uint32_t)(hash ^ (hash >> 32));
#else
        uint32_t accId = 12345678;
#endif

        accId &= 0x7FFFFFFF;
        if (accId < 10000000)
            accId += 10000000;

        // Ensure we never land on the buggy hardcoded ID
        if (accId == 1792139526)
            accId = 1792139527;

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
        if (fopen_s(&f, "steam_autogen_id.dat", "wb") == 0 && f)
        {
            fwrite(&g_AccountID, sizeof(g_AccountID), 1, f);
            fclose(f);
        }
        if (fopen_s(&f, "cstrike\\steam_autogen_id.dat", "wb") == 0 && f)
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

        // 28-byte ticket recognized by Reunion and DProto (SC2009 / AVSMP)
        t->header_len = 0x14; // 20
        t->unk1 = 0;
        t->unk2 = 0;
        t->account_id = g_AccountID; // 1792139526 -> STEAM_0:0:896069763
        t->unk3 = 0;
        t->unk4 = 0;
        t->unk5 = 0;

        return sizeof(SteamEmuAuthTicket);
    }
}
