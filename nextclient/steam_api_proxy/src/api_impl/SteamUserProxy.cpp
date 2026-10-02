#include "SteamUserProxy.h"
#include "../voice/SpeexVoiceManager.h"

#ifdef _WIN32
#include <windows.h>

static int DetectCallerClientIndex()
{
    __try
    {
        uintptr_t caller_ebp = 0;
#if defined(_M_IX86)
        __asm
        {
            mov eax, [ebp]
            mov caller_ebp, eax
        }
#endif
        if (caller_ebp != 0 && !IsBadReadPtr(reinterpret_cast<void*>(caller_ebp), 16))
        {
            // In hw.dll!Voice_AddIncomingData(int clientIndex, ...), parameter 1 is at [caller_ebp + 8]
            int candidate = *reinterpret_cast<int*>(caller_ebp + 8);
            if (candidate >= 0 && candidate < SpeexVoiceManager::kMaxPlayers)
            {
                return candidate;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return 0;
}
#else
static int DetectCallerClientIndex()
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

    // 1. Primary path: Attempt original Steam Voice (SILK/Opus) decompression
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

    // 2. Fallback path: Native Steam Voice failed or output 0 bytes.
    // The payload is almost certainly a legacy Speex bitstream from a classic CS 1.6 client (build 4554).
    int clientIndex = DetectCallerClientIndex();
    uint32 speexWritten = 0;
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
            &speexWritten,
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
        &speexWritten,
        nDesiredSampleRate
    );
#endif

    if (decoded && speexWritten > 0)
    {
        if (nBytesWritten)
            *nBytesWritten = speexWritten;

        return k_EVoiceResultOK;
    }

    return res;
}
