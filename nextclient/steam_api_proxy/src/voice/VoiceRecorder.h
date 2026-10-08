#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <steam/isteamuser.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

class VoiceRecorder
{
public:
    static VoiceRecorder& GetInstance();

    bool Init();
    void Shutdown();

    void StartRecording();
    void StopRecording();
    bool IsRecording() const { return m_isRecording.load(); }

    EVoiceResult GetAvailableVoice(uint32* pcbCompressed, uint32* pcbUncompressed, uint32 nUncompressedVoiceDesiredSampleRate);
    EVoiceResult GetVoice(
        bool bWantCompressed,
        void* pDestBuffer,
        uint32 cbDestBufferSize,
        uint32* nBytesWritten,
        bool bWantUncompressed,
        void* pUncompressedDestBuffer,
        uint32 cbUncompressedDestBufferSize,
        uint32* nUncompressBytesWritten,
        uint32 nUncompressedVoiceDesiredSampleRate
    );

private:
    VoiceRecorder();
    ~VoiceRecorder();

    VoiceRecorder(const VoiceRecorder&) = delete;
    VoiceRecorder& operator=(const VoiceRecorder&) = delete;

#ifdef _WIN32
    static DWORD WINAPI RecordThreadStub(LPVOID param);
    void RecordThreadProc();
#endif

    struct EncodedFrame
    {
        uint16_t seq{0};
        std::vector<uint8_t> opus_data;
        std::vector<int16_t> raw_pcm;
    };

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_threadRunning{false};

    uint16_t m_currentSeq{0};

    void* m_opusEncoder{nullptr};
    void* m_resampler{nullptr};

#ifdef _WIN32
    HWAVEIN m_hWaveIn{nullptr};
    HANDLE m_hWaveEvent{nullptr};
    HANDLE m_hThread{nullptr};

    static constexpr int kOpusRate = 24000;
    static constexpr int kNumWaveBuffers = 4;
    static constexpr int kFrameSamples = 480; // 20 ms @ 24000 Hz
    static constexpr int kWaveBufferSize = kFrameSamples * sizeof(int16_t);

    WAVEHDR m_waveHeaders[kNumWaveBuffers]{};
    int16_t m_waveBuffers[kNumWaveBuffers][kFrameSamples]{};

#endif

    std::mutex m_mutex;
    std::deque<EncodedFrame> m_frameQueue;
};
