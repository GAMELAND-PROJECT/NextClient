#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "SpeexVoiceManager.h"
#include <cstring>
#include <cstdlib>
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

static uint32_t CalculateCRC32(const void* buf, size_t len)
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

SpeexVoiceManager::ChannelState::ChannelState()
{
    Init();
}

SpeexVoiceManager::ChannelState::~ChannelState()
{
    if (opus_dec)
    {
        std::free(opus_dec);
        opus_dec = nullptr;
    }

    if (silk_dec)
    {
        std::free(silk_dec);
        silk_dec = nullptr;
    }

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

    int postfilter = 1;
    if (nb_dec)
    {
        speex_decoder_ctl(nb_dec, SPEEX_SET_ENH, &postfilter);
        int rate = kNarrowbandRate;
        speex_decoder_ctl(nb_dec, SPEEX_SET_SAMPLING_RATE, &rate);
    }

    if (wb_dec)
    {
        speex_decoder_ctl(wb_dec, SPEEX_SET_ENH, &postfilter);
        int rate = kWidebandRate;
        speex_decoder_ctl(wb_dec, SPEEX_SET_SAMPLING_RATE, &rate);
    }

    int err = 0;
    // Resampler quality 4: superb audio fidelity with sharp anti-aliasing cutoff and low latency
    resampler_nb = speex_resampler_init(1, kNarrowbandRate, kGoldSrcSampleRate, 4, &err);
    resampler_wb = speex_resampler_init(1, kWidebandRate, kGoldSrcSampleRate, 4, &err);

    // Opus decoder initialization (8000 Hz, 1 channel mono)
    int opusSize = opus_decoder_get_size(1);
    opus_dec = static_cast<OpusDecoder*>(std::malloc(opusSize));
    if (opus_dec)
    {
        opus_decoder_init(opus_dec, 8000, 1);
    }
    opus_seq = 0;
    opus_seq_init = false;

    // Silk decoder initialization (8000 Hz mono)
    int silkSize = 0;
    SKP_Silk_SDK_Get_Decoder_Size(&silkSize);
    silk_dec = std::malloc(silkSize);
    if (silk_dec)
    {
        SKP_Silk_SDK_InitDecoder(silk_dec);
    }
    std::memset(&silk_control, 0, sizeof(silk_control));
    silk_control.API_sampleRate = 8000;

    last_packet_time_ms = 0;
    detected_frame_size = 0;
    leftover_count = 0;
    initialized = true;
}

void SpeexVoiceManager::ChannelState::ResetOpus()
{
    if (opus_dec)
    {
        opus_decoder_ctl(opus_dec, OPUS_RESET_STATE);
    }
    opus_seq = 0;
    opus_seq_init = false;
}

void SpeexVoiceManager::ChannelState::ResetSilk()
{
    if (silk_dec)
    {
        SKP_Silk_SDK_InitDecoder(silk_dec);
    }
    std::memset(&silk_control, 0, sizeof(silk_control));
    silk_control.API_sampleRate = 8000;
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

    ResetOpus();
    ResetSilk();

    if (resampler_nb)
        speex_resampler_reset_mem(resampler_nb);

    if (resampler_wb)
        speex_resampler_reset_mem(resampler_wb);

    leftover_count = 0;
}

SpeexVoiceManager::SpeexVoiceManager()
{
    for (int i = 0; i < kTotalChannels; ++i)
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
    if (clientIndex >= 0 && clientIndex < kTotalChannels)
    {
        m_channels[clientIndex].ResetState();
    }
}

void SpeexVoiceManager::ResetAll()
{
    for (int i = 0; i < kTotalChannels; ++i)
    {
        m_channels[i].ResetState();
    }
}

static int DetectSpeexFrameSize(const uint8_t* pData, uint32_t totalBytes)
{
    if (!pData || totalBytes == 0)
        return 0;

    const uint8_t b0 = pData[0];
    const int wideband = (b0 >> 7) & 1;
    const int submode = (b0 >> 3) & 0x0F;

    if (!wideband)
    {
        // GoldSrc Speex Narrowband fixed frame sizes:
        // Submode 5: Quality 5 -> 38 bytes per frame (ReVoice default SPEEX_VOICE_QUALITY 5)
        if (submode == 5 && (totalBytes % 38 == 0 || totalBytes >= 38))
            return 38;

        // Submode 4: Quality 4 -> 28 bytes per frame (sv_voicequality 4)
        if (submode == 4 && (totalBytes % 28 == 0 || totalBytes >= 28))
            return 28;

        // Submode 3: Quality 3 -> 20 bytes per frame (CS 1.6 standard sv_voicequality 3 default)
        if (submode == 3 && (totalBytes % 20 == 0 || totalBytes >= 20))
            return 20;

        // Submode 2: Quality 2 -> 15 bytes per frame (sv_voicequality 2)
        if (submode == 2 && (totalBytes % 15 == 0 || totalBytes >= 15))
            return 15;

        // Submode 1: Quality 1 -> 6 bytes per frame
        if (submode == 1 && (totalBytes % 6 == 0 || totalBytes >= 6))
            return 6;
    }

    // Fallback divisibility check matching GoldSrc voice_speex quality table
    if (totalBytes % 20 == 0) return 20;
    if (totalBytes % 38 == 0) return 38;
    if (totalBytes % 28 == 0) return 28;
    if (totalBytes % 15 == 0) return 15;
    if (totalBytes % 6 == 0) return 6;

    if (totalBytes >= 38 && (totalBytes % 38 < 8)) return 38;
    if (totalBytes >= 20 && (totalBytes % 20 < 6)) return 20;

    return 20;
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
    if (pBytesWritten)
        *pBytesWritten = 0;

    if (!pCompressed || cbCompressed == 0 || !pDestBuffer || cbDestBufferSize == 0)
        return false;

    if (clientIndex < 0 || clientIndex >= kTotalChannels)
        clientIndex = 0;

    ChannelState& channel = m_channels[clientIndex];
    if (!channel.initialized)
        channel.Init();

    const uint32_t now = GetTickCount();
    // Reset state after silence gap (> 800ms) to ensure clean filter states
    if (channel.last_packet_time_ms != 0 && (now - channel.last_packet_time_ms > 800))
    {
        channel.ResetState();
    }
    channel.last_packet_time_ms = now;

    if (nDesiredSampleRate == 0)
        nDesiredSampleRate = kGoldSrcSampleRate;

    // Stack buffer for decoded PCM frames (zero heap allocation in audio callback)
    int16_t pcm_intermediate[kMaxPcmSamples];
    int total_pcm_samples = 0;

    // Prepend any leftover samples from previous packet to preserve phase continuity
    if (channel.leftover_count > 0 && channel.leftover_count <= kMaxLeftover)
    {
        std::memcpy(pcm_intermediate, channel.leftover_pcm, channel.leftover_count * sizeof(int16_t));
        total_pcm_samples = static_cast<int>(channel.leftover_count);
        channel.leftover_count = 0;
    }

    const uint8_t* pByteData = static_cast<const uint8_t*>(pCompressed);
    int in_sample_rate = kNarrowbandRate;
    SpeexResamplerState* active_resampler = channel.resampler_nb;

    // =========================================================================
    // Path 1: Steam P2P Container (Opus / Silk / Speex wire packets from ReVoice / Steam)
    // =========================================================================
    if (cbCompressed >= 12)
    {
        uint32_t id_low = *reinterpret_cast<const uint32_t*>(pByteData);
        uint32_t id_high = *reinterpret_cast<const uint32_t*>(pByteData + 4);

        bool isSteamP2P = false;
        // Check for ReVoice fake SteamID (0x00000011, 0x01100001) or Steam individual account universe
        if (id_low == 0x00000011 && ((id_high & 0xFFF00000) == 0x01100000 || id_high == 0x01100001))
        {
            isSteamP2P = true;
        }
        else if ((id_high & 0xFFF00000) == 0x01100000)
        {
            isSteamP2P = true;
        }
        else if (cbCompressed >= 13)
        {
            // Opcode check at byte 8: 11 (SamplingRate), 6 (OPUS_PLC), 5 (OPUS), 4 (Silk), 2 (Speex), 0 (Silence)
            uint8_t op = pByteData[8];
            if (op == 11 || op == 6 || op == 5 || op == 4 || op == 2 || op == 0)
            {
                uint32_t wireCrc = *reinterpret_cast<const uint32_t*>(pByteData + cbCompressed - 4);
                uint32_t calcCrc = CalculateCRC32(pCompressed, cbCompressed - 4);
                if (calcCrc == wireCrc)
                {
                    isSteamP2P = true;
                }
            }
        }

        if (isSteamP2P)
        {
            const uint8_t* pStream = pByteData + 8;
            const uint8_t* pStreamEnd = pByteData + cbCompressed - 4;
            int streamSampleRate = kNarrowbandRate;

            while (pStream < pStreamEnd)
            {
                uint8_t opcode = *pStream++;

                switch (opcode)
                {
                case 11: // PLT_SamplingRate
                {
                    if (pStream + 2 > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }
                    streamSampleRate = *reinterpret_cast<const uint16_t*>(pStream);
                    pStream += 2;
                    break;
                }

                case 6: // PLT_OPUS_PLC (Opus with Packet Loss Concealment framing)
                {
                    if (pStream + 2 > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }
                    uint16_t streamLen = *reinterpret_cast<const uint16_t*>(pStream);
                    pStream += 2;
                    if (pStream + streamLen > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }

                    const uint8_t* pOpus = pStream;
                    const uint8_t* pOpusEnd = pStream + streamLen;

                    while (pOpus < pOpusEnd && channel.opus_dec)
                    {
                        if (pOpus + 2 > pOpusEnd) break;
                        uint16_t nPayloadSize = *reinterpret_cast<const uint16_t*>(pOpus);
                        pOpus += 2;

                        if (nPayloadSize == 0xFFFF)
                        {
                            channel.ResetOpus();
                            break;
                        }

                        if (pOpus + 2 > pOpusEnd) break;
                        uint16_t nCurSeq = *reinterpret_cast<const uint16_t*>(pOpus);
                        pOpus += 2;

                        if (!channel.opus_seq_init || nCurSeq < channel.opus_seq)
                        {
                            channel.ResetOpus();
                            channel.opus_seq = nCurSeq;
                            channel.opus_seq_init = true;
                        }
                        else if (nCurSeq > channel.opus_seq)
                        {
                            int nLoss = nCurSeq - channel.opus_seq;
                            if (nLoss > 10) nLoss = 10;
                            for (int i = 0; i < nLoss && total_pcm_samples + 160 <= kMaxPcmSamples; ++i)
                            {
                                int n = opus_decode(channel.opus_dec, nullptr, 0, reinterpret_cast<opus_int16*>(&pcm_intermediate[total_pcm_samples]), 160, 0);
                                if (n > 0) total_pcm_samples += n;
                            }
                        }
                        channel.opus_seq = nCurSeq + 1;

                        if (pOpus + nPayloadSize > pOpusEnd) break;

                        if (nPayloadSize == 0)
                        {
                            // DTX / Silence
                            if (total_pcm_samples + 160 <= kMaxPcmSamples)
                            {
                                std::memset(&pcm_intermediate[total_pcm_samples], 0, 160 * sizeof(int16_t));
                                total_pcm_samples += 160;
                            }
                        }
                        else
                        {
                            if (total_pcm_samples + 480 <= kMaxPcmSamples)
                            {
                                int n = opus_decode(channel.opus_dec, pOpus, nPayloadSize, reinterpret_cast<opus_int16*>(&pcm_intermediate[total_pcm_samples]), 480, 0);
                                if (n > 0) total_pcm_samples += n;
                            }
                        }
                        pOpus += nPayloadSize;
                    }

                    pStream += streamLen;
                    in_sample_rate = streamSampleRate > 0 ? streamSampleRate : kNarrowbandRate;
                    active_resampler = (in_sample_rate == kWidebandRate) ? channel.resampler_wb : channel.resampler_nb;
                    break;
                }

                case 5: // PLT_OPUS (Legacy Opus framing without sequence number)
                {
                    if (pStream + 2 > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }
                    uint16_t streamLen = *reinterpret_cast<const uint16_t*>(pStream);
                    pStream += 2;
                    if (pStream + streamLen > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }

                    const uint8_t* pOpus = pStream;
                    const uint8_t* pOpusEnd = pStream + streamLen;

                    while (pOpus < pOpusEnd && channel.opus_dec)
                    {
                        if (pOpus + 2 > pOpusEnd) break;
                        uint16_t nPayloadSize = *reinterpret_cast<const uint16_t*>(pOpus);
                        pOpus += 2;

                        if (nPayloadSize == 0xFFFF)
                        {
                            channel.ResetOpus();
                            break;
                        }

                        if (pOpus + nPayloadSize > pOpusEnd) break;

                        if (nPayloadSize == 0)
                        {
                            if (total_pcm_samples + 160 <= kMaxPcmSamples)
                            {
                                std::memset(&pcm_intermediate[total_pcm_samples], 0, 160 * sizeof(int16_t));
                                total_pcm_samples += 160;
                            }
                        }
                        else
                        {
                            if (total_pcm_samples + 480 <= kMaxPcmSamples)
                            {
                                int n = opus_decode(channel.opus_dec, pOpus, nPayloadSize, reinterpret_cast<opus_int16*>(&pcm_intermediate[total_pcm_samples]), 480, 0);
                                if (n > 0) total_pcm_samples += n;
                            }
                        }
                        pOpus += nPayloadSize;
                    }

                    pStream += streamLen;
                    in_sample_rate = streamSampleRate > 0 ? streamSampleRate : kNarrowbandRate;
                    active_resampler = (in_sample_rate == kWidebandRate) ? channel.resampler_wb : channel.resampler_nb;
                    break;
                }

                case 4: // PLT_Silk
                {
                    if (pStream + 2 > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }
                    uint16_t streamLen = *reinterpret_cast<const uint16_t*>(pStream);
                    pStream += 2;
                    if (pStream + streamLen > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }

                    const uint8_t* pSilk = pStream;
                    const uint8_t* pSilkEnd = pStream + streamLen;

                    while (pSilk < pSilkEnd && channel.silk_dec)
                    {
                        if (pSilk + 2 > pSilkEnd) break;
                        uint16_t nPayloadSize = *reinterpret_cast<const uint16_t*>(pSilk);
                        pSilk += 2;

                        if (nPayloadSize == 0xFFFF)
                        {
                            channel.ResetSilk();
                            break;
                        }

                        if (pSilk + nPayloadSize > pSilkEnd) break;

                        channel.silk_control.API_sampleRate = 8000;
                        do
                        {
                            short nSamples = static_cast<short>(kMaxPcmSamples - total_pcm_samples);
                            if (nSamples <= 0) break;
                            int ret = SKP_Silk_SDK_Decode(
                                channel.silk_dec,
                                &channel.silk_control,
                                0,
                                pSilk,
                                nPayloadSize,
                                &pcm_intermediate[total_pcm_samples],
                                &nSamples
                            );
                            if (ret != SKP_SILK_NO_ERROR) break;
                            total_pcm_samples += nSamples;
                        } while (channel.silk_control.moreInternalDecoderFrames);

                        pSilk += nPayloadSize;
                    }

                    pStream += streamLen;
                    in_sample_rate = kNarrowbandRate;
                    active_resampler = channel.resampler_nb;
                    break;
                }

                case 2: // PLT_Speex (Speex payload within container)
                {
                    if (pStream + 2 > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }
                    uint16_t streamLen = *reinterpret_cast<const uint16_t*>(pStream);
                    pStream += 2;
                    if (pStream + streamLen > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }

                    speex_bits_read_from(&channel.bits, reinterpret_cast<const char*>(pStream), streamLen);
                    while (speex_bits_remaining(&channel.bits) >= 10 && total_pcm_samples + kNbFrameSamples <= kMaxPcmSamples)
                    {
                        int16_t frame_buf[kNbFrameSamples];
                        int ret = speex_decode_int(channel.nb_dec, &channel.bits, frame_buf);
                        if (ret != 0) break;
                        std::memcpy(&pcm_intermediate[total_pcm_samples], frame_buf, kNbFrameSamples * sizeof(int16_t));
                        total_pcm_samples += kNbFrameSamples;
                    }

                    pStream += streamLen;
                    in_sample_rate = kNarrowbandRate;
                    active_resampler = channel.resampler_nb;
                    break;
                }

                case 0: // PLT_Silence
                {
                    if (pStream + 2 > pStreamEnd)
                    {
                        pStream = pStreamEnd;
                        break;
                    }
                    uint16_t numSilence = *reinterpret_cast<const uint16_t*>(pStream);
                    pStream += 2;
                    if (total_pcm_samples + numSilence <= kMaxPcmSamples)
                    {
                        std::memset(&pcm_intermediate[total_pcm_samples], 0, numSilence * sizeof(int16_t));
                        total_pcm_samples += numSilence;
                    }
                    break;
                }

                default:
                    // Unknown opcode, terminate container parse
                    pStream = pStreamEnd;
                    break;
                }
            }
        }
    }

    // =========================================================================
    // Path 2: Raw Speex Bitstream (Classic CS 1.6 builds 3248, 4554, pure HLDS)
    // =========================================================================
    if (total_pcm_samples == 0)
    {
        const int is_wideband = (pByteData[0] >> 7) & 1;

        if (!is_wideband)
        {
            int frame_size = DetectSpeexFrameSize(pByteData, cbCompressed);
            if (frame_size <= 0)
                frame_size = 20;

            channel.detected_frame_size = frame_size;

            uint32_t offset = 0;
            int frame_count = 0;

            while (offset + frame_size <= cbCompressed && frame_count < kMaxFramesPerPacket)
            {
                const char* frame_ptr = reinterpret_cast<const char*>(pByteData + offset);

                speex_bits_read_from(&channel.bits, frame_ptr, frame_size);

                int16_t frame_buf[kNbFrameSamples];
                int ret = speex_decode_int(channel.nb_dec, &channel.bits, frame_buf);
                if (ret != 0)
                    break;

                if (total_pcm_samples + kNbFrameSamples <= kMaxPcmSamples)
                {
                    std::memcpy(&pcm_intermediate[total_pcm_samples], frame_buf, kNbFrameSamples * sizeof(int16_t));
                    total_pcm_samples += kNbFrameSamples;
                }

                offset += frame_size;
                frame_count++;
            }

            in_sample_rate = kNarrowbandRate;
            active_resampler = channel.resampler_nb;
        }

        // Fallback: If narrowband failed to produce samples, try wideband
        if (total_pcm_samples == 0)
        {
            speex_bits_read_from(&channel.bits, static_cast<const char*>(pCompressed), static_cast<int>(cbCompressed));
            int frame_count = 0;

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

            if (total_pcm_samples > 0)
            {
                in_sample_rate = kWidebandRate;
                active_resampler = channel.resampler_wb;
            }
        }
    }

    if (total_pcm_samples == 0)
        return false;

    // =========================================================================
    // Resampling Path: Convert from in_sample_rate to nDesiredSampleRate (11025 Hz)
    // =========================================================================
    if (in_sample_rate == static_cast<int>(nDesiredSampleRate))
    {
        uint32_t bytes_to_copy = (std::min)(
            static_cast<uint32_t>(total_pcm_samples * sizeof(int16_t)),
            cbDestBufferSize
        );
        bytes_to_copy &= ~1u;

        std::memcpy(pDestBuffer, pcm_intermediate, bytes_to_copy);
        if (pBytesWritten)
            *pBytesWritten = bytes_to_copy;

        return bytes_to_copy > 0;
    }

    if (!active_resampler)
        return false;

    speex_resampler_set_rate(active_resampler, in_sample_rate, nDesiredSampleRate);

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

    // Save unconsumed samples into leftover_pcm to maintain continuous waveform
    if (in_len < static_cast<uint32_t>(total_pcm_samples))
    {
        uint32_t rem = static_cast<uint32_t>(total_pcm_samples) - in_len;
        if (rem > kMaxLeftover)
            rem = kMaxLeftover;

        std::memcpy(channel.leftover_pcm, &pcm_intermediate[in_len], rem * sizeof(int16_t));
        channel.leftover_count = rem;
    }
    else
    {
        channel.leftover_count = 0;
    }

    uint32_t out_bytes = (std::min)(
        static_cast<uint32_t>(out_len * sizeof(int16_t)),
        cbDestBufferSize
    );
    out_bytes &= ~1u;

    if (out_bytes == 0)
        return false;

    std::memcpy(pDestBuffer, pcm_resampled, out_bytes);
    if (pBytesWritten)
        *pBytesWritten = out_bytes;

    return true;
}
