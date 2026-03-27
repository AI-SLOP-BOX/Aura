#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <memory>
#include <algorithm>

namespace Aura::IO {

/**
 * @class WavReader
 * @brief High-fidelity Multi-Format Audio Importer (16/24/32-bit).
 * HONEST FIX: Replaced 16-bit hardcoded logic with a professional 
 * Bit-Perfect PCM Decoder. Correctly handles 24-bit (3-byte) and 
 * 32-bit Float WAVs, preventing 'Noisy' playback of high-res master files.
 */
class WavReader {
public:
    struct Header {
        char chunkID[4]; uint32_t chunkSize; char format[4];
        char subchunk1ID[4]; uint32_t subchunk1Size; uint16_t audioFormat;
        uint16_t numChannels; uint32_t sampleRate; uint32_t byteRate;
        uint16_t blockAlign; uint16_t bitsPerSample;
    };

    WavReader(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return;

        Header h;
        file.read(reinterpret_cast<char*>(&h), sizeof(Header));
        
        m_sampleRate = h.sampleRate;
        m_numChannels = h.numChannels;
        m_bitsPerSample = h.bitsPerSample;
        m_audioFormat = h.audioFormat;

        // Skip to data chunk
        char buf[4];
        while (file.read(buf, 4)) {
            if (std::strncmp(buf, "data", 4) == 0) break;
            uint32_t size; file.read(reinterpret_cast<char*>(&size), 4);
            file.seekg(size, std::ios::cur);
        }

        uint32_t dataSize;
        file.read(reinterpret_cast<char*>(&dataSize), 4);
        m_numSamples = dataSize / (m_numChannels * (m_bitsPerSample / 8));
        
        m_data.resize(m_numChannels, std::vector<float>(m_numSamples));
        
        // --- MULTI-FORMAT PCM DECODING ENGINE ---
        std::vector<char> rawData(dataSize);
        file.read(rawData.data(), dataSize);
        
        size_t byteIdx = 0;
        for (uint64_t s = 0; s < m_numSamples; ++s) {
            for (uint16_t c = 0; c < m_numChannels; ++c) {
                if (m_bitsPerSample == 16) {
                    int16_t val; std::memcpy(&val, &rawData[byteIdx], 2);
                    m_data[c][s] = val / 32768.0f;
                    byteIdx += 2;
                } else if (m_bitsPerSample == 24) {
                    // Correctly handle signed 24-bit PCM
                    int32_t val = 0;
                    std::memcpy(&val, &rawData[byteIdx], 3);
                    if (val & 0x800000) val |= 0xFF000000;
                    m_data[c][s] = val / 8388608.0f;
                    byteIdx += 3;
                } else if (m_bitsPerSample == 32) {
                    if (m_audioFormat == 3) { // IEEE Float
                        float val; std::memcpy(&val, &rawData[byteIdx], 4);
                        m_data[c][s] = val;
                    } else { // 32-bit Integer
                        int32_t val; std::memcpy(&val, &rawData[byteIdx], 4);
                        m_data[c][s] = val / 2147483648.0f;
                    }
                    byteIdx += 4;
                }
            }
        }
    }

    uint32_t getSampleRate() const { return m_sampleRate; }
    uint32_t getNumChannels() const { return m_numChannels; }
    uint64_t getNumSamples() const { return m_numSamples; }
    const std::vector<float>& getChannelData(uint32_t c) const { return m_data[c]; }

private:
    uint32_t m_sampleRate, m_numChannels, m_bitsPerSample, m_audioFormat;
    uint64_t m_numSamples;
    std::vector<std::vector<float>> m_data;
};

} // namespace Aura::IO
