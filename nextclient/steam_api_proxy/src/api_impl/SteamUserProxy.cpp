#include "SteamUserProxy.h"
#include "SteamEmu.h"
#include "../voice/SpeexVoiceManager.h"
#include "../voice/VoiceRecorder.h"
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>

static int DetectCallerClientIndex(const void* pExpectedCompressed, uint32 cbExpectedCompressed)
{
    (void)cbExpectedCompressed;
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

            const void* cand_ptr = *reinterpret_cast<const void**>(cur_ebp + 12);

            if (cand_ptr == pExpectedCompressed)
            {
                int idx = *reinterpret_cast<int*>(cur_ebp + 8);
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
    (void)pExpectedCompressed;
    (void)cbExpectedCompressed;
    return 0;
}
#endif

SteamUserVoiceProxy g_SteamUserVoiceProxy;

int SteamUserVoiceProxy::InitiateGameConnection(
    void *pAuthBlob,
    int cbMaxAuthBlob,
    CSteamID steamIDGameServer,
    uint32 unIPServer,
    uint16 usPortServer,
    bool bSecure
)
{
    (void)steamIDGameServer;
    (void)bSecure;

    if (!pAuthBlob || cbMaxAuthBlob < (int)sizeof(SteamEmuAuthTicket))
        return 0;

    int res = (int)SteamEmu::GenerateAuthTicket(pAuthBlob, (size_t)cbMaxAuthBlob, unIPServer, usPortServer);

    FILE* f = fopen("platform\\steam\\steam_auth.log", "a");
    if (!f)
        f = fopen("steam_auth.log", "a");
    if (f)
    {
        uint32_t acc = SteamEmu::GetAccountID();
        fprintf(f, "[SteamEmu] InitiateGameConnection: unIPServer=0x%08X usPortServer=%u TicketLen=%d SteamID=STEAM_0:%u:%u\n",
            unIPServer, usPortServer, res, acc & 1, acc / 2);
        fclose(f);
    }

    return res;
}

bool SteamUserVoiceProxy::GetUserDataFolder(char *pchBuffer, int cubBuffer)
{
    if (!pchBuffer || cubBuffer <= 0)
        return false;

#ifdef _WIN32
    strncpy_s(pchBuffer, cubBuffer, "cstrike", _TRUNCATE);
#else
    strncpy(pchBuffer, "cstrike", cubBuffer);
    pchBuffer[cubBuffer - 1] = '\0';
#endif
    return true;
}

HAuthTicket SteamUserVoiceProxy::GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket)
{
    if (!pTicket || cbMaxTicket < (int)sizeof(SteamEmuAuthTicket))
    {
        if (pcbTicket) *pcbTicket = 0;
        return k_HAuthTicketInvalid;
    }

    size_t sz = SteamEmu::GenerateAuthTicket(pTicket, (size_t)cbMaxTicket, 0, 0);
    if (pcbTicket)
        *pcbTicket = (uint32)sz;

    return 1;
}

void SteamUserVoiceProxy::StartVoiceRecording()
{
    if (m_pOrig)
    {
        m_pOrig->StartVoiceRecording();
        return;
    }
    VoiceRecorder::GetInstance().StartRecording();
}

void SteamUserVoiceProxy::StopVoiceRecording()
{
    if (m_pOrig)
    {
        m_pOrig->StopVoiceRecording();
        return;
    }
    VoiceRecorder::GetInstance().StopRecording();
}

EVoiceResult SteamUserVoiceProxy::GetAvailableVoice(
    uint32 *pcbCompressed,
    uint32 *pcbUncompressed,
    uint32 nUncompressedVoiceDesiredSampleRate
)
{
    if (m_pOrig)
    {
        return m_pOrig->GetAvailableVoice(
            pcbCompressed,
            pcbUncompressed,
            nUncompressedVoiceDesiredSampleRate
        );
    }
    return VoiceRecorder::GetInstance().GetAvailableVoice(
        pcbCompressed,
        pcbUncompressed,
        nUncompressedVoiceDesiredSampleRate
    );
}

EVoiceResult SteamUserVoiceProxy::GetVoice(
    bool bWantCompressed,
    void *pDestBuffer,
    uint32 cbDestBufferSize,
    uint32 *nBytesWritten,
    bool bWantUncompressed,
    void *pUncompressedDestBuffer,
    uint32 cbUncompressedDestBufferSize,
    uint32 *nUncompressBytesWritten,
    uint32 nUncompressedVoiceDesiredSampleRate
)
{
    if (m_pOrig)
    {
        return m_pOrig->GetVoice(
            bWantCompressed,
            pDestBuffer,
            cbDestBufferSize,
            nBytesWritten,
            bWantUncompressed,
            pUncompressedDestBuffer,
            cbUncompressedDestBufferSize,
            nUncompressBytesWritten,
            nUncompressedVoiceDesiredSampleRate
        );
    }
    return VoiceRecorder::GetInstance().GetVoice(
        bWantCompressed,
        pDestBuffer,
        cbDestBufferSize,
        nBytesWritten,
        bWantUncompressed,
        pUncompressedDestBuffer,
        cbUncompressedDestBufferSize,
        nUncompressBytesWritten,
        nUncompressedVoiceDesiredSampleRate
    );
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

    // 1. Primary path: Attempt original SmartSteamEmu / Steamworks voice decompressor
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
        if (res == k_EVoiceResultOK && nBytesWritten && *nBytesWritten > 0)
        {
            return k_EVoiceResultOK;
        }
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

    return (res == k_EVoiceResultOK) ? k_EVoiceResultOK : k_EVoiceResultDataCorrupted;
}

