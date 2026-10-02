#include "SteamUserProxy.h"
#include "../voice/SpeexVoiceManager.h"

#ifdef _WIN32
#include <windows.h>

static int DetectCallerClientIndex(const void* pExpectedCompressed, uint32 cbExpectedCompressed)
{
#if defined(_M_IX86)
    __try
    {
        uintptr_t cur_ebp = 0;
        __asm
        {
            mov eax, [ebp]
            mov cur_ebp, eax
        }

        // Walk up the EBP chain to locate hw.dll!Voice_AddIncomingData frame
        for (int depth = 0; depth < 8 && cur_ebp != 0; ++depth)
        {
            if (IsBadReadPtr(reinterpret_cast<void*>(cur_ebp), 32))
                break;

            // In hw.dll!Voice_AddIncomingData:
            // [cur_ebp + 8]  = clientIndex (int)
            // [cur_ebp + 12] = pCompressed (void*)
            // Note: [cur_ebp + 16] was originally cbCompressed, but hw.dll zeroes it
            // before calling DecompressVoice (mov [ebp+10h], ebx) and reuses it as
            // &nBytesWritten. Therefore we must only verify cand_ptr == pExpectedCompressed.
            const void* cand_ptr = *reinterpret_cast<const void**>(cur_ebp + 12);

            if (cand_ptr == pExpectedCompressed)
            {
                int idx = *reinterpret_cast<int*>(cur_ebp + 8);
                // 156 / 0x9C / -100 is local microphone loopback
                if (idx == 156 || idx == 0x9C || idx == -100)
                {
                    return SpeexVoiceManager::kLoopbackChannel;
                }
                if (idx >= 0 && idx < SpeexVoiceManager::kMaxPlayers)
                {
                    return idx;
                }
            }

            cur_ebp = *reinterpret_cast<uintptr_t*>(cur_ebp);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#endif
    return 0;
}
#else
static int DetectCallerClientIndex(const void* pExpectedCompressed, uint32 cbExpectedCompressed)
{
    return 0;
}
#endif

SteamUserVoiceProxy g_SteamUserVoiceProxy;

SteamUserVoiceProxy::SteamUserVoiceProxy()
    : m_pOrig(nullptr)
{
}

EVoiceResult SteamUserVoiceProxy::DecompressVoice(
    const void *pCompressed,
    uint32 cbCompressed,
    void *pDestBuffer,
    uint32 cbDestBufferSize,
    uint32 *nBytesWritten,
    uint32 nDesiredSampleRate
)
{
    if (nBytesWritten)
        *nBytesWritten = 0;

    if (!pCompressed || cbCompressed == 0 || !pDestBuffer || cbDestBufferSize == 0)
        return k_EVoiceResultNoData;

    // 1. Primary path: Attempt original Steamworks Voice decompression (if official Steam is running)
    EVoiceResult res = k_EVoiceResultDataCorrupted;
    if (m_pOrig)
    {
#if defined(_WIN32) && defined(_MSC_VER)
        __try
        {
            res = m_pOrig->DecompressVoice(
                pCompressed,
                cbCompressed,
                pDestBuffer,
                cbDestBufferSize,
                nBytesWritten,
                nDesiredSampleRate
            );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            res = k_EVoiceResultDataCorrupted;
        }
#else
        res = m_pOrig->DecompressVoice(
            pCompressed,
            cbCompressed,
            pDestBuffer,
            cbDestBufferSize,
            nBytesWritten,
            nDesiredSampleRate
        );
#endif
    }

    // If native Steam Voice successfully decoded audio bytes, return immediately
    if (res == k_EVoiceResultOK && nBytesWritten && *nBytesWritten > 0)
    {
        return k_EVoiceResultOK;
    }

    // 2. Universal Voice Decoder Path:
    // Decodes Opus, Silk, Speex (builds 3248, 4554, ReVoice, Steam) with per-player isolated channels
    int clientIndex = DetectCallerClientIndex(pCompressed, cbCompressed);
    uint32 voiceWritten = 0;
    bool decoded = false;

#if defined(_WIN32) && defined(_MSC_VER)
    __try
    {
        decoded = SpeexVoiceManager::GetInstance().DecodeVoice(
            clientIndex,
            pCompressed,
            cbCompressed,
            pDestBuffer,
            cbDestBufferSize,
            &voiceWritten,
            nDesiredSampleRate
        );
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        decoded = false;
    }
#else
    decoded = SpeexVoiceManager::GetInstance().DecodeVoice(
        clientIndex,
        pCompressed,
        cbCompressed,
        pDestBuffer,
        cbDestBufferSize,
        &voiceWritten,
        nDesiredSampleRate
    );
#endif

    if (decoded && voiceWritten > 0)
    {
        if (nBytesWritten)
            *nBytesWritten = voiceWritten;

        return k_EVoiceResultOK;
    }

    return res;
}
