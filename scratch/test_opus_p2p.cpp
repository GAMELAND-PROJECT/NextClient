#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <opus/opus.h>

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

int main()
{
    int err = 0;
    OpusEncoder* enc = opus_encoder_create(8000, 1, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !enc)
    {
        std::cout << "Failed to create opus encoder: " << err << std::endl;
        return 1;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc, OPUS_SET_DTX(0));

    // Generate 160 samples (20 ms at 8000 Hz) of 440 Hz sine wave
    int16_t pcm[160];
    for (int i = 0; i < 160; ++i)
    {
        pcm[i] = static_cast<int16_t>(10000.0 * sin(2.0 * 3.1415926535 * 440.0 * i / 8000.0));
    }

    uint8_t opus_data[256];
    int opus_bytes = opus_encode(enc, pcm, 160, opus_data, sizeof(opus_data));
    std::cout << "Encoded 160 samples to " << opus_bytes << " bytes of Opus." << std::endl;

    // Build Steam P2P packet
    std::vector<uint8_t> packet;
    // 1. SteamID (8 bytes)
    uint32_t id_low = 0x00000011;
    uint32_t id_high = 0x01100001;
    packet.insert(packet.end(), (uint8_t*)&id_low, (uint8_t*)&id_low + 4);
    packet.insert(packet.end(), (uint8_t*)&id_high, (uint8_t*)&id_high + 4);

    // 2. Opcode 11 (SamplingRate)
    packet.push_back(11);
    uint16_t rate = 8000;
    packet.insert(packet.end(), (uint8_t*)&rate, (uint8_t*)&rate + 2);

    // 3. Opcode 6 (PLT_OPUS_PLC)
    packet.push_back(6);
    uint16_t stream_len = 2 + 2 + opus_bytes;
    packet.insert(packet.end(), (uint8_t*)&stream_len, (uint8_t*)&stream_len + 2);

    // 4. Opus frame
    uint16_t payload_size = opus_bytes;
    uint16_t seq = 0;
    packet.insert(packet.end(), (uint8_t*)&payload_size, (uint8_t*)&payload_size + 2);
    packet.insert(packet.end(), (uint8_t*)&seq, (uint8_t*)&seq + 2);
    packet.insert(packet.end(), opus_data, opus_data + opus_bytes);

    // 5. CRC32
    uint32_t crc = CalculateCRC32(packet.data(), packet.size());
    packet.insert(packet.end(), (uint8_t*)&crc, (uint8_t*)&crc + 4);

    std::cout << "Total Steam P2P packet size: " << packet.size() << " bytes." << std::endl;

    // Now test decoding with Opus decoder!
    OpusDecoder* dec = opus_decoder_create(8000, 1, &err);
    int16_t decoded_pcm[160];
    int dec_samples = opus_decode(dec, opus_data, opus_bytes, decoded_pcm, 160, 0);
    std::cout << "Decoded " << dec_samples << " samples from Opus!" << std::endl;

    opus_encoder_destroy(enc);
    opus_decoder_destroy(dec);

    if (dec_samples == 160)
    {
        std::cout << "SUCCESS! Perfect round-trip encode/decode!" << std::endl;
        return 0;
    }
    return 2;
}
