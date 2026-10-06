#include "VoiceRecorder.h"
#include <cstring>
#include <algorithm>
#include <opus/opus.h>
#include <speex/speex_resampler.h>

namespace {

uint32_t CalculateCRC32(const void* buf, size_t len)
{
    static uint32_t table[256];
    static bool table_init = false;
    if (!table_init)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
            {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        table_init = true;
    }

    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < len; ++i)
    {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

} // namespace

VoiceRecorder& VoiceRecorder::GetInstance()
{
    static VoiceRecorder instance;
    return instance;
}

VoiceRecorder::VoiceRecorder()
{
}

VoiceRecorder::~VoiceRecorder()
{
    Shutdown();
}

bool VoiceRecorder::Init()
{
    if (m_initialized.load())
        return true;

    // 1. Initialize Opus Encoder (8000 Hz, mono VOIP)
    int err = 0;
    OpusEncoder* enc = opus_encoder_create(8000, 1, OPUS_APPLICATION_VOIP, &err);
    if (err == OPUS_OK && enc)
    {
        opus_encoder_ctl(enc, OPUS_SET_BITRATE(24000));
        opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
        opus_encoder_ctl(enc, OPUS_SET_DTX(0));
        m_opusEncoder = enc;
    }

    // 2. Initialize Speex Resampler (from 8000 Hz to 11025 Hz for uncompressed engine loopback)
    int resErr = 0;
    m_resampler = speex_resampler_init(1, 8000, 11025, 3, &resErr);

#ifdef _WIN32
    // 3. Initialize Windows waveIn Audio Capture
    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;
    wfx.nAvgBytesPerSec = 16000;
    wfx.nBlockAlign = 2;
    wfx.wBitsPerSample = 16;
    wfx.cbSize = 0;

    m_hWaveEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_hWaveEvent)
    {
        m_initialized = true;
        return false;
    }

    MMRESULT mmres = waveInOpen(
        &m_hWaveIn,
        WAVE_MAPPER,
        &wfx,
        reinterpret_cast<DWORD_PTR>(m_hWaveEvent),
        0,
        CALLBACK_EVENT
    );

    if (mmres != MMSYSERR_NOERROR || !m_hWaveIn)
    {
        m_hWaveIn = nullptr;
        CloseHandle(m_hWaveEvent);
        m_hWaveEvent = nullptr;
        m_initialized = true;
        return false;
    }

    for (int i = 0; i < kNumWaveBuffers; ++i)
    {
        m_waveHeaders[i].lpData = reinterpret_cast<LPSTR>(m_waveBuffers[i]);
        m_waveHeaders[i].dwBufferLength = kWaveBufferSize;
        m_waveHeaders[i].dwBytesRecorded = 0;
        m_waveHeaders[i].dwFlags = 0;
        m_waveHeaders[i].dwUser = 0;
        waveInPrepareHeader(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));
        waveInAddBuffer(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));
    }

    m_threadRunning = true;
    m_hThread = CreateThread(nullptr, 0, RecordThreadStub, this, 0, nullptr);
#endif

    m_initialized = true;
    return true;
}

void VoiceRecorder::Shutdown()
{
    m_isRecording = false;

#ifdef _WIN32
    if (m_threadRunning.load())
    {
        m_threadRunning = false;
        if (m_hWaveEvent)
            SetEvent(m_hWaveEvent);

        if (m_hThread)
        {
            WaitForSingleObject(m_hThread, 500);
            CloseHandle(m_hThread);
            m_hThread = nullptr;
        }
    }

    if (m_hWaveIn)
    {
        waveInStop(m_hWaveIn);
        waveInReset(m_hWaveIn);
        for (int i = 0; i < kNumWaveBuffers; ++i)
        {
            waveInUnprepareHeader(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));
        }
        waveInClose(m_hWaveIn);
        m_hWaveIn = nullptr;
    }

    if (m_hWaveEvent)
    {
        CloseHandle(m_hWaveEvent);
        m_hWaveEvent = nullptr;
    }
#endif

    if (m_opusEncoder)
    {
        opus_encoder_destroy(reinterpret_cast<OpusEncoder*>(m_opusEncoder));
        m_opusEncoder = nullptr;
    }

    if (m_resampler)
    {
        speex_resampler_destroy(reinterpret_cast<SpeexResamplerState*>(m_resampler));
        m_resampler = nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameQueue.clear();
    m_initialized = false;
}

void VoiceRecorder::StartRecording()
{
    if (!m_initialized.load())
    {
        Init();
    }

#ifdef _WIN32
    if (m_hWaveIn)
    {
        m_isRecording = true;
        waveInStart(m_hWaveIn);
    }
#endif
}

void VoiceRecorder::StopRecording()
{
#ifdef _WIN32
    if (m_hWaveIn)
    {
        m_isRecording = false;
        waveInStop(m_hWaveIn);
    }
#endif
}

#ifdef _WIN32
DWORD WINAPI VoiceRecorder::RecordThreadStub(LPVOID param)
{
    static_cast<VoiceRecorder*>(param)->RecordThreadProc();
    return 0;
}

void VoiceRecorder::RecordThreadProc()
{
    while (m_threadRunning.load())
    {
        DWORD wr = WaitForSingleObject(m_hWaveEvent, 50);
        if (!m_threadRunning.load())
            break;

        if (wr == WAIT_OBJECT_0 && m_hWaveIn)
        {
            for (int i = 0; i < kNumWaveBuffers; ++i)
            {
                if (m_waveHeaders[i].dwFlags & WHDR_DONE)
                {
                    if (m_isRecording.load() && m_waveHeaders[i].dwBytesRecorded >= kWaveBufferSize)
                    {
                        int16_t* pcm = m_waveBuffers[i];

                        if (m_opusEncoder)
                        {
                            uint8_t opusBuf[256];
                            int nBytes = opus_encode(
                                reinterpret_cast<OpusEncoder*>(m_opusEncoder),
                                pcm,
                                kFrameSamples,
                                opusBuf,
                                sizeof(opusBuf)
                            );

                            if (nBytes > 0)
                            {
                                std::lock_guard<std::mutex> lock(m_mutex);
                                if (m_frameQueue.size() > 50)
                                {
                                    m_frameQueue.pop_front();
                                }

                                EncodedFrame frame;
                                frame.seq = m_currentSeq++;
                                frame.opus_data.assign(opusBuf, opusBuf + nBytes);
                                frame.raw_pcm.assign(pcm, pcm + kFrameSamples);
                                m_frameQueue.push_back(std::move(frame));
                            }
                        }
                    }

                    m_waveHeaders[i].dwFlags &= ~WHDR_DONE;
                    waveInAddBuffer(m_hWaveIn, &m_waveHeaders[i], sizeof(WAVEHDR));
                }
            }
        }
    }
}
#endif

EVoiceResult VoiceRecorder::GetAvailableVoice(
    uint32* pcbCompressed,
    uint32* pcbUncompressed,
    uint32 nUncompressedVoiceDesiredSampleRate
)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_frameQueue.empty())
    {
        if (pcbCompressed) *pcbCompressed = 0;
        if (pcbUncompressed) *pcbUncompressed = 0;
        return m_isRecording.load() ? k_EVoiceResultNoData : k_EVoiceResultNotRecording;
    }

    size_t numFrames = std::min<size_t>(m_frameQueue.size(), 10);
    uint32_t totalOpusBytes = 0;
    for (size_t i = 0; i < numFrames; ++i)
    {
        totalOpusBytes += 4 + static_cast<uint32_t>(m_frameQueue[i].opus_data.size());
    }

    // Header: 8 bytes SteamID + 3 bytes (Opcode 11 + sample rate) + 3 bytes (Opcode 6 + stream len)
    // Footer: 4 bytes CRC32
    uint32_t totalComp = 8 + 3 + 3 + totalOpusBytes + 4;
    if (pcbCompressed)
        *pcbCompressed = totalComp;

    if (pcbUncompressed)
    {
        uint32_t rate = (nUncompressedVoiceDesiredSampleRate > 0) ? nUncompressedVoiceDesiredSampleRate : 11025;
        uint32_t uncompSamples = static_cast<uint32_t>(numFrames * kFrameSamples);
        *pcbUncompressed = (uncompSamples * rate / 8000) * sizeof(int16_t);
    }

    return k_EVoiceResultOK;
}

EVoiceResult VoiceRecorder::GetVoice(
    bool bWantCompressed,
    void* pDestBuffer,
    uint32 cbDestBufferSize,
    uint32* nBytesWritten,
    bool bWantUncompressed,
    void* pUncompressedDestBuffer,
    uint32 cbUncompressedDestBufferSize,
    uint32* nUncompressBytesWritten,
    uint32 nUncompressedVoiceDesiredSampleRate
)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_frameQueue.empty())
    {
        if (nBytesWritten) *nBytesWritten = 0;
        if (nUncompressBytesWritten) *nUncompressBytesWritten = 0;
        return m_isRecording.load() ? k_EVoiceResultNoData : k_EVoiceResultNotRecording;
    }

    size_t numFrames = std::min<size_t>(m_frameQueue.size(), 10);
    size_t framesWritten = 0;

    if (bWantCompressed && pDestBuffer && cbDestBufferSize >= 32)
    {
        uint8_t* pOut = static_cast<uint8_t*>(pDestBuffer);
        uint32_t maxOut = cbDestBufferSize;

        // 1. SteamID (8 bytes): ReVoice universal SteamID header (0x00000011, 0x01100001)
        uint32_t id_low = 0x00000011;
        uint32_t id_high = 0x01100001;
        std::memcpy(pOut, &id_low, 4);
        std::memcpy(pOut + 4, &id_high, 4);
        uint32_t pos = 8;

        // 2. Opcode 11 (SamplingRate = 8000)
        pOut[pos++] = 11;
        uint16_t rate = 8000;
        std::memcpy(pOut + pos, &rate, 2);
        pos += 2;

        // 3. Opcode 6 (PLT_OPUS_PLC)
        pOut[pos++] = 6;
        uint32_t lenFieldPos = pos;
        pos += 2; // placeholder for total Opus stream len

        uint32_t opusStreamStart = pos;
        for (size_t i = 0; i < numFrames; ++i)
        {
            const auto& frame = m_frameQueue[i];
            uint32_t frameTotal = 4 + static_cast<uint32_t>(frame.opus_data.size());
            if (pos + frameTotal + 4 > maxOut)
                break;

            uint16_t pSize = static_cast<uint16_t>(frame.opus_data.size());
            uint16_t pSeq = frame.seq;
            std::memcpy(pOut + pos, &pSize, 2); pos += 2;
            std::memcpy(pOut + pos, &pSeq, 2); pos += 2;
            std::memcpy(pOut + pos, frame.opus_data.data(), frame.opus_data.size());
            pos += static_cast<uint32_t>(frame.opus_data.size());
            framesWritten++;
        }

        uint16_t totalOpusStreamLen = static_cast<uint16_t>(pos - opusStreamStart);
        std::memcpy(pOut + lenFieldPos, &totalOpusStreamLen, 2);

        // 4. Checksum CRC32 (4 bytes)
        uint32_t crc = CalculateCRC32(pOut, pos);
        std::memcpy(pOut + pos, &crc, 4);
        pos += 4;

        if (nBytesWritten)
            *nBytesWritten = pos;
    }
    else
    {
        if (nBytesWritten)
            *nBytesWritten = 0;
    }

    // 5. Uncompressed PCM resampled to desired sample rate (for GoldSrc loopback)
    if (bWantUncompressed && pUncompressedDestBuffer && cbUncompressedDestBufferSize > 0 && framesWritten > 0)
    {
        std::vector<int16_t> pcmCombined;
        for (size_t i = 0; i < framesWritten; ++i)
        {
            pcmCombined.insert(pcmCombined.end(), m_frameQueue[i].raw_pcm.begin(), m_frameQueue[i].raw_pcm.end());
        }

        if (m_resampler && !pcmCombined.empty())
        {
            uint32_t inLen = static_cast<uint32_t>(pcmCombined.size());
            uint32_t outLen = cbUncompressedDestBufferSize / sizeof(int16_t);
            speex_resampler_process_int(
                reinterpret_cast<SpeexResamplerState*>(m_resampler),
                0,
                pcmCombined.data(),
                &inLen,
                static_cast<spx_int16_t*>(pUncompressedDestBuffer),
                &outLen
            );
            if (nUncompressBytesWritten)
                *nUncompressBytesWritten = outLen * sizeof(int16_t);
        }
        else
        {
            if (nUncompressBytesWritten)
                *nUncompressBytesWritten = 0;
        }
    }
    else
    {
        if (nUncompressBytesWritten)
            *nUncompressBytesWritten = 0;
    }

    // Pop the written frames from queue
    for (size_t i = 0; i < framesWritten; ++i)
    {
        m_frameQueue.pop_front();
    }

    return (framesWritten > 0) ? k_EVoiceResultOK : k_EVoiceResultNoData;
}
