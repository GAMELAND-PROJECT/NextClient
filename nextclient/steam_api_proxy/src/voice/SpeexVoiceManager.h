#pragma once

#include <cstdint>
#include <speex/speex.h>
#include <speex/speex_resampler.h>
#include <opus/opus.h>
#include <silk/SKP_Silk_SDK_API.h>

class SpeexVoiceManager
{
public:
    static constexpr int kMaxPlayers = 32;
    static constexpr int kLoopbackChannel = 32;
    static constexpr int kTotalChannels = 33;
    static constexpr int kNarrowbandRate = 8000;
    static constexpr int kWidebandRate = 16000;
    static constexpr int kGoldSrcSampleRate = 11025;
    static constexpr int kNbFrameSamples = 160;
    static constexpr int kWbFrameSamples = 320;
    static constexpr int kMaxFramesPerPacket = 16;
    static constexpr int kMaxLeftover = 64;
    static constexpr int kMaxPcmSamples = 4096;

    struct ChannelState
    {
        // Speex
        void* nb_dec{nullptr};
        void* wb_dec{nullptr};
        SpeexBits bits{};

        // Opus
        OpusDecoder* opus_dec{nullptr};
        uint16_t opus_seq{0};
        bool opus_seq_init{false};

        // Silk
        void* silk_dec{nullptr};
        SKP_SILK_SDK_DecControlStruct silk_control{};

        // Resampling
        SpeexResamplerState* resampler_nb{nullptr};
        SpeexResamplerState* resampler_wb{nullptr};
        uint32_t last_packet_time_ms{0};
        int detected_frame_size{0};
        int16_t leftover_pcm[kMaxLeftover]{0};
        uint32_t leftover_count{0};
        bool initialized{false};

        ChannelState();
        ~ChannelState();
        void Init();
        void ResetState();
        void ResetOpus();
        void ResetSilk();
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

    ChannelState m_channels[kTotalChannels];
};
