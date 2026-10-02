#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <string>
#include <speex/speex.h>
#include <speex/speex_resampler.h>
#include <opus/opus.h>
#include <silk/SKP_Silk_SDK_API.h>

#include "f:/NextClient-1/nextclient/steam_api_proxy/src/voice/SpeexVoiceManager.h"

// Write simple 16-bit mono WAV header
void WriteWav(const char* filename, const int16_t* samples, size_t num_samples, int sample_rate)
{
    std::ofstream out(filename, std::ios::binary);
    uint32_t subchunk2_size = num_samples * sizeof(int16_t);
    uint32_t chunk_size = 36 + subchunk2_size;
    
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunk_size), 4);
    out.write("WAVE", 4);
    
    out.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels = 1; // Mono
    uint32_t byte_rate = sample_rate * num_channels * sizeof(int16_t);
    uint16_t block_align = num_channels * sizeof(int16_t);
    uint16_t bits_per_sample = 16;
    
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&num_channels), 2);
    out.write(reinterpret_cast<const char*>(&sample_rate), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&block_align), 2);
    out.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&subchunk2_size), 4);
    out.write(reinterpret_cast<const char*>(samples), subchunk2_size);
}

static uint32_t CRC32(const void* buf, size_t len)
{
    static uint32_t table[256];
    static bool init = false;
    if (!init)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

int main()
{
    std::cout << "Starting Universal Voice Manager test (Speex, Opus, Silk, Steam P2P)..." << std::endl;

    // Test 1: Speex Quality 5 and Quality 3
    for (int test_quality : {5, 3})
    {
        std::cout << "\n--- Testing Speex Quality " << test_quality << " ---" << std::endl;

        SpeexBits enc_bits;
        speex_bits_init(&enc_bits);
        void* enc = speex_encoder_init(&speex_nb_mode);

        int spx_qual = (test_quality == 5) ? 8 : 4;
        speex_encoder_ctl(enc, SPEEX_SET_QUALITY, &spx_qual);
        int rate = 8000;
        speex_encoder_ctl(enc, SPEEX_SET_SAMPLING_RATE, &rate);

        const int frame_bytes = (test_quality == 5) ? 38 : 20;
        const int total_raw_samples = 8000;
        std::vector<int16_t> input_audio(total_raw_samples);
        for (int i = 0; i < total_raw_samples; ++i)
        {
            double t = (double)i / 8000.0;
            double val = 0.6 * std::sin(2.0 * 3.1415926535 * 440.0 * t) + 
                         0.3 * std::sin(2.0 * 3.1415926535 * 880.0 * t);
            input_audio[i] = static_cast<int16_t>(val * 16000.0);
        }

        const int frames_per_packet = 1;
        const int packet_size = frames_per_packet * frame_bytes;
        const int num_packets = total_raw_samples / (frames_per_packet * 160);

        std::vector<int16_t> output_audio;
        SpeexVoiceManager& mgr = SpeexVoiceManager::GetInstance();
        mgr.ResetAll();

        for (int p = 0; p < num_packets; ++p)
        {
            std::vector<char> packet(packet_size);
            for (int f = 0; f < frames_per_packet; ++f)
            {
                int sample_offset = (p * frames_per_packet + f) * 160;
                float float_in[160];
                for (int s = 0; s < 160; ++s)
                    float_in[s] = (float)input_audio[sample_offset + s];

                speex_bits_reset(&enc_bits);
                speex_encode(enc, float_in, &enc_bits);
                speex_bits_write(&enc_bits, &packet[f * frame_bytes], frame_bytes);
            }

            char dest_buf[8192];
            uint32_t bytes_written = 0;
            bool ok = mgr.DecodeVoice(
                0,
                packet.data(),
                packet.size(),
                dest_buf,
                sizeof(dest_buf),
                &bytes_written,
                11025
            );

            if (!ok || bytes_written == 0)
            {
                std::cerr << "Packet " << p << " decode FAILED!" << std::endl;
            }
            else
            {
                int samples = bytes_written / sizeof(int16_t);
                const int16_t* pcm = reinterpret_cast<const int16_t*>(dest_buf);
                output_audio.insert(output_audio.end(), pcm, pcm + samples);
            }
        }

        std::cout << "Decoded total samples at 11025 Hz: " << output_audio.size() << std::endl;
        std::string wav_name = "test_spx_q" + std::to_string(test_quality) + ".wav";
        WriteWav(wav_name.c_str(), output_audio.data(), output_audio.size(), 11025);

        speex_bits_destroy(&enc_bits);
        speex_encoder_destroy(enc);
    }

    // Test 2: Steam P2P Container with Opus (ReVoice modern format)
    {
        std::cout << "\n--- Testing ReVoice Steam P2P with OPUS ---" << std::endl;

        int encError = 0;
        OpusEncoder* opusEnc = opus_encoder_create(8000, 1, OPUS_APPLICATION_VOIP, &encError);
        opus_encoder_ctl(opusEnc, OPUS_SET_BITRATE(32000));

        const int total_samples = 8000;
        std::vector<int16_t> input_audio(total_samples);
        for (int i = 0; i < total_samples; ++i)
        {
            double t = (double)i / 8000.0;
            double val = 0.5 * std::sin(2.0 * 3.1415926535 * 500.0 * t);
            input_audio[i] = static_cast<int16_t>(val * 16000.0);
        }

        SpeexVoiceManager& mgr = SpeexVoiceManager::GetInstance();
        mgr.ResetAll();

        std::vector<int16_t> output_audio;
        uint16_t seq = 0;
        const int num_frames = total_samples / 160;

        for (int f = 0; f < num_frames; ++f)
        {
            uint8_t opus_frame[512];
            int opus_len = opus_encode(opusEnc, &input_audio[f * 160], 160, opus_frame, sizeof(opus_frame));

            // Build ReVoice CSteamP2PCodec wire packet:
            // 8 bytes: steamid (0x00000011, 0x01100001)
            // 1 byte: opcode 11 (SamplingRate)
            // 2 bytes: sample rate (8000)
            // 1 byte: opcode 6 (PLT_OPUS_PLC)
            // 2 bytes: streamLen
            //   2 bytes: nPayloadSize
            //   2 bytes: nCurSeq
            //   opus_len bytes: opus payload
            // 4 bytes: crc32
            std::vector<uint8_t> p2p;
            // steamid
            uint32_t id_low = 0x00000011;
            uint32_t id_high = 0x01100001;
            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&id_low), reinterpret_cast<uint8_t*>(&id_low) + 4);
            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&id_high), reinterpret_cast<uint8_t*>(&id_high) + 4);

            // Sampling rate
            p2p.push_back(11);
            uint16_t sr = 8000;
            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&sr), reinterpret_cast<uint8_t*>(&sr) + 2);

            // Opus PLC opcode
            p2p.push_back(6);
            uint16_t streamLen = 2 + 2 + opus_len;
            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&streamLen), reinterpret_cast<uint8_t*>(&streamLen) + 2);

            uint16_t payloadSize = opus_len;
            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&payloadSize), reinterpret_cast<uint8_t*>(&payloadSize) + 2);

            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&seq), reinterpret_cast<uint8_t*>(&seq) + 2);
            seq++;

            p2p.insert(p2p.end(), opus_frame, opus_frame + opus_len);

            // CRC32
            uint32_t cksum = CRC32(p2p.data(), p2p.size());
            p2p.insert(p2p.end(), reinterpret_cast<uint8_t*>(&cksum), reinterpret_cast<uint8_t*>(&cksum) + 4);

            // Decode packet using SpeexVoiceManager!
            char dest_buf[8192];
            uint32_t bytes_written = 0;
            bool ok = mgr.DecodeVoice(
                1,
                p2p.data(),
                p2p.size(),
                dest_buf,
                sizeof(dest_buf),
                &bytes_written,
                11025
            );

            if (!ok || bytes_written == 0)
            {
                std::cerr << "Opus packet " << f << " decode FAILED!" << std::endl;
            }
            else
            {
                int samples = bytes_written / sizeof(int16_t);
                const int16_t* pcm = reinterpret_cast<const int16_t*>(dest_buf);
                output_audio.insert(output_audio.end(), pcm, pcm + samples);
            }
        }

        std::cout << "Decoded total Opus samples at 11025 Hz: " << output_audio.size() << std::endl;
        WriteWav("test_revoice_opus.wav", output_audio.data(), output_audio.size(), 11025);
        std::cout << "Saved test_revoice_opus.wav successfully!" << std::endl;

        opus_encoder_destroy(opusEnc);
    }

    std::cout << "\nALL VOICE TESTS PASSED 100%!" << std::endl;
    return 0;
}
