#pragma once

#include <cstdint>
#include <speex/speex.h>
#include <speex/speex_resampler.h>

class SpeexVoiceManager
{
public:
    static constexpr int kMaxPlayers = 32;
    static constexpr int kNarrowbandRate = 8000;
    static constexpr int kWidebandRate = 16000;
    static constexpr int kGoldSrcSampleRate = 11025;
    static constexpr int kNbFrameSamples = 160;
    static constexpr int kWbFrameSamples = 320;
    static constexpr int kMaxFramesPerPacket = 16;
    static constexpr int kMaxPcmSamples = kWbFrameSamples * kMaxFramesPerPacket;

    struct ChannelState
    {
        void* nb_dec{nullptr};
        void* wb_dec{nullptr};
        SpeexBits bits{};
        SpeexResamplerState* resampler_nb{nullptr};
        SpeexResamplerState* resampler_wb{nullptr};
        uint32_t last_packet_time_ms{0};
        bool initialized{false};

        ChannelState();
        ~ChannelState();
        void Init();
        void ResetState();
    };

    static SpeexVoiceManager& GetInstance();

    bool DecodeVoice(
        int clientIndex,
        const void* pCompressed,
        uint32_t cbCompressed,
        void* pDestBuffer,
        uint32_t cbDestBufferSize,
        uint32_t* pBytesWritten,
        uint32_t nDesiredSampleRate
    );

    void ResetChannel(int clientIndex);
    void ResetAll();

private:
    SpeexVoiceManager();
    ~SpeexVoiceManager();

    SpeexVoiceManager(const SpeexVoiceManager&) = delete;
    SpeexVoiceManager& operator=(const SpeexVoiceManager&) = delete;

    ChannelState m_channels[kMaxPlayers];
};
