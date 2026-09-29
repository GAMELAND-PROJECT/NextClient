#include "turn_speed_patch.h"
#include <nitro_utils/platform.h>
#include <nitro_utils/MemoryTools.h>
#include <cstdint>
#include <cstddef>

#ifdef _WIN32
// Allows user to set cl_yawspeed and cl_pitchspeed up to 1000.0f
// (Original CS 1.6 client.dll hardcodes clamp to 250.0f)
static float g_MaxTurnSpeed = 1000.0f;

void TurnSpeedLimitPatch()
{
    MemoryModule module("client.dll");
    if (!module.Module())
        return;

    uint8_t* base = reinterpret_cast<uint8_t*>(module.Start());
    size_t size = module.Size();
    if (!base || size < 0x60000)
        return;

    // Pattern for the clamp routine in client.dll:
    // 8B 0D ?? ?? ?? ?? D9 41 0C D8 1D ?? ?? ?? ?? DF E0 F6 C4 05 7B ?? D9 41 0C D8 1D
    // In CS 1.6 client.dll, this pattern appears exactly twice:
    // 1st occurrence: cl_yawspeed clamp
    // 2nd occurrence: cl_pitchspeed clamp
    static constexpr uint8_t kPattern[] = {
        0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00,
        0xD9, 0x41, 0x0C,
        0xD8, 0x1D, 0x00, 0x00, 0x00, 0x00,
        0xDF, 0xE0,
        0xF6, 0xC4, 0x05,
        0x7B, 0x00,
        0xD9, 0x41, 0x0C,
        0xD8, 0x1D
    };
    static constexpr uint8_t kMask[] = {
        1, 1, 0, 0, 0, 0,
        1, 1, 1,
        1, 1, 0, 0, 0, 0,
        1, 1,
        1, 1, 1,
        1, 0,
        1, 1, 1,
        1, 1
    };

    for (size_t i = 0; i + sizeof(kPattern) + 60 < size; ++i)
    {
        bool match = true;
        for (size_t j = 0; j < sizeof(kPattern); ++j)
        {
            if (kMask[j] && base[i + j] != kPattern[j])
            {
                match = false;
                break;
            }
        }
        if (!match)
            continue;

        // Verify jp opcode (0x7A) at offset i + 36
        if (base[i + 36] != 0x7A)
            continue;

        int8_t disp = static_cast<int8_t>(base[i + 37]);
        size_t target = i + 38 + disp;
        if (target + 8 >= size)
            continue;

        // Check if Yaw clamp: target has D9 05 (fld dword ptr [ptr])
        if (base[target] == 0xD9 && base[target + 1] == 0x05)
        {
            // If already pointing to g_MaxTurnSpeed, skip
            if (*reinterpret_cast<uintptr_t*>(&base[i + 27]) == reinterpret_cast<uintptr_t>(&g_MaxTurnSpeed))
                continue;

            // Patch comparison pointer at i + 27
            nitro_utils::SetProtect(&base[i + 27], 4, nitro_utils::ProtectMode::PROTECT_RWE);
            *reinterpret_cast<uintptr_t*>(&base[i + 27]) = reinterpret_cast<uintptr_t>(&g_MaxTurnSpeed);
            nitro_utils::SetProtect(&base[i + 27], 4, nitro_utils::ProtectMode::PROTECT_RE);

            // Patch fld pointer at target + 2
            nitro_utils::SetProtect(&base[target + 2], 4, nitro_utils::ProtectMode::PROTECT_RWE);
            *reinterpret_cast<uintptr_t*>(&base[target + 2]) = reinterpret_cast<uintptr_t>(&g_MaxTurnSpeed);
            nitro_utils::SetProtect(&base[target + 2], 4, nitro_utils::ProtectMode::PROTECT_RE);
        }
        // Check if Pitch clamp: target has C7 44 24 04 (mov dword ptr [esp+4], imm32)
        else if (base[target] == 0xC7 && base[target + 1] == 0x44 && base[target + 2] == 0x24)
        {
            // If already pointing to g_MaxTurnSpeed, skip
            if (*reinterpret_cast<uintptr_t*>(&base[i + 27]) == reinterpret_cast<uintptr_t>(&g_MaxTurnSpeed))
                continue;

            // Patch comparison pointer at i + 27
            nitro_utils::SetProtect(&base[i + 27], 4, nitro_utils::ProtectMode::PROTECT_RWE);
            *reinterpret_cast<uintptr_t*>(&base[i + 27]) = reinterpret_cast<uintptr_t>(&g_MaxTurnSpeed);
            nitro_utils::SetProtect(&base[i + 27], 4, nitro_utils::ProtectMode::PROTECT_RE);

            // Patch immediate float at target + 4
            nitro_utils::SetProtect(&base[target + 4], 4, nitro_utils::ProtectMode::PROTECT_RWE);
            *reinterpret_cast<float*>(&base[target + 4]) = 1000.0f;
            nitro_utils::SetProtect(&base[target + 4], 4, nitro_utils::ProtectMode::PROTECT_RE);
        }
    }
}
#else
void TurnSpeedLimitPatch()
{
}
#endif
