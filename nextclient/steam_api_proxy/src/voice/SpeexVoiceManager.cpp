#include "SpeexVoiceManager.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <chrono>
static uint32_t GetTickCount()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
#endif

SpeexVoiceManager::ChannelState::ChannelState()
{
    Init();
}

SpeexVoiceManager::ChannelState::~ChannelState()
{
    if (resampler_nb)
    {
        speex_resampler_destroy(resampler_nb);
        resampler_nb = nullptr;
    }

    if (resampler_wb)
    {
        speex_resampler_destroy(resampler_wb);
        resampler_wb = nullptr;
    }

    if (nb_dec)
    {
        speex_decoder_destroy(nb_dec);
        nb_dec = nullptr;
    }

    if (wb_dec)
    {
        speex_decoder_destroy(wb_dec);
        wb_dec = nullptr;
    }

    speex_bits_destroy(&bits);
    initialized = false;
}

void SpeexVoiceManager::ChannelState::Init()
{
    if (initialized)
        return;

    speex_bits_init(&bits);

    nb_dec = speex_decoder_init(&speex_nb_mode);
    wb_dec = speex_decoder_init(&speex_wb_mode);

    int err = 0;
    resampler_nb = speex_resampler_init(1, kNarrowbandRate, kGoldSrcSampleRate, 3, &err);
    resampler_wb = speex_resampler_init(1, kWidebandRate, kGoldSrcSampleRate, 3, &err);

    last_packet_time_ms = 0;
    initialized = true;
}

void SpeexVoiceManager::ChannelState::ResetState()
{
    if (!initialized)
        return;

    speex_bits_reset(&bits);

    if (nb_dec)
        speex_decoder_ctl(nb_dec, SPEEX_RESET_STATE, nullptr);

    if (wb_dec)
        speex_decoder_ctl(wb_dec, SPEEX_RESET_STATE, nullptr);

    if (resampler_nb)
        speex_resampler_reset_mem(resampler_nb);

    if (resampler_wb)
        speex_resampler_reset_mem(resampler_wb);
}

SpeexVoiceManager::SpeexVoiceManager()
{
    for (int i = 0; i < kMaxPlayers; ++i)
    {
        m_channels[i].Init();
    }
}

SpeexVoiceManager::~SpeexVoiceManager()
{
}

SpeexVoiceManager& SpeexVoiceManager::GetInstance()
{
    static SpeexVoiceManager instance;
    return instance;
}

void SpeexVoiceManager::ResetChannel(int clientIndex)
{
    if (clientIndex >= 0 && clientIndex < kMaxPlayers)
    {
        m_channels[clientIndex].ResetState();
    }
}

void SpeexVoiceManager::ResetAll()
{
    for (int i = 0; i < kMaxPlayers; ++i)
    {
        m_channels[i].ResetState();
    }
}

bool SpeexVoiceManager::DecodeVoice(
    int clientIndex,
    const void* pCompressed,
    uint32_t cbCompressed,
    void* pDestBuffer,
    uint32_t cbDestBufferSize,
    uint32_t* pBytesWritten,
    uint32_t nDesiredSampleRate
)
{
    if (!pCompressed || cbCompressed == 0 || !pDestBuffer || cbDestBufferSize == 0)
        return false;

    if (clientIndex < 0 || clientIndex >= kMaxPlayers)
        clientIndex = 0;

    ChannelState& channel = m_channels[clientIndex];
    if (!channel.initialized)
        channel.Init();

    const uint32_t now = GetTickCount();
    // Reset state after silence (> 800ms gap) to prevent LPC history bleeding
    if (now - channel.last_packet_time_ms > 800)
    {
        channel.ResetState();
    }
    channel.last_packet_time_ms = now;

    if (nDesiredSampleRate == 0)
        nDesiredSampleRate = kGoldSrcSampleRate;

    // Stack buffer for decoded PCM frames (up to 16 frames = 320ms, zero heap allocation)
    int16_t pcm_intermediate[kMaxPcmSamples];
    int total_pcm_samples = 0;

    speex_bits_read_from(&channel.bits, static_cast<const char*>(pCompressed), static_cast<int>(cbCompressed));

    // Try Narrowband (8 kHz) decoding first (standard CS 1.6 4554)
    int frame_count = 0;
    bool nb_success = false;

    while (speex_bits_remaining(&channel.bits) >= 10 && frame_count < kMaxFramesPerPacket)
    {
        int16_t frame_buf[kNbFrameSamples];
        int ret = speex_decode_int(channel.nb_dec, &channel.bits, frame_buf);
        if (ret != 0)
            break;

        if (total_pcm_samples + kNbFrameSamples <= kMaxPcmSamples)
        {
            std::memcpy(&pcm_intermediate[total_pcm_samples], frame_buf, kNbFrameSamples * sizeof(int16_t));
            total_pcm_samples += kNbFrameSamples;
            nb_success = true;
        }
        frame_count++;
    }

    int in_sample_rate = kNarrowbandRate;
    SpeexResamplerState* active_resampler = channel.resampler_nb;

    // Fallback: If narrowband failed on the first frame, test if it's wideband
    if (!nb_success || total_pcm_samples == 0)
    {
        speex_bits_read_from(&channel.bits, static_cast<const char*>(pCompressed), static_cast<int>(cbCompressed));
        total_pcm_samples = 0;
        frame_count = 0;

        while (speex_bits_remaining(&channel.bits) >= 10 && frame_count < kMaxFramesPerPacket)
        {
            int16_t frame_buf[kWbFrameSamples];
            int ret = speex_decode_int(channel.wb_dec, &channel.bits, frame_buf);
            if (ret != 0)
                break;

            if (total_pcm_samples + kWbFrameSamples <= kMaxPcmSamples)
            {
                std::memcpy(&pcm_intermediate[total_pcm_samples], frame_buf, kWbFrameSamples * sizeof(int16_t));
                total_pcm_samples += kWbFrameSamples;
            }
            frame_count++;
        }

        in_sample_rate = kWidebandRate;
        active_resampler = channel.resampler_wb;
    }

    if (total_pcm_samples == 0)
        return false;

    // Handle resampling to nDesiredSampleRate (typically 11025 Hz for GoldSrc)
    if (in_sample_rate == static_cast<int>(nDesiredSampleRate))
    {
        const uint32_t bytes_to_copy = std::min(
            static_cast<uint32_t>(total_pcm_samples * sizeof(int16_t)),
            cbDestBufferSize
        );

        std::memcpy(pDestBuffer, pcm_intermediate, bytes_to_copy);
        if (pBytesWritten)
            *pBytesWritten = bytes_to_copy;

        return bytes_to_copy > 0;
    }

    if (!active_resampler)
        return false;

    speex_resampler_set_rate(active_resampler, in_sample_rate, nDesiredSampleRate);

    // Resample into stack output buffer
    constexpr int kMaxOutSamples = 4096;
    int16_t pcm_resampled[kMaxOutSamples];

    uint32_t in_len = static_cast<uint32_t>(total_pcm_samples);
    uint32_t out_len = static_cast<uint32_t>(kMaxOutSamples);

    speex_resampler_process_int(
        active_resampler,
        0,
        pcm_intermediate,
        &in_len,
        pcm_resampled,
        &out_len
    );

    const uint32_t out_bytes = std::min(
        static_cast<uint32_t>(out_len * sizeof(int16_t)),
        cbDestBufferSize
    );

    if (out_bytes == 0)
        return false;

    std::memcpy(pDestBuffer, pcm_resampled, out_bytes);
    if (pBytesWritten)
        *pBytesWritten = out_bytes;

    return true;
}
