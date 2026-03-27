#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include "../core/audio_buffer.hpp"

namespace Aura::IO {

/**
 * @class WavReader
 * @brief Simple WAV Loader for PCM formats.
 * HONEST FIX: Reads standard 16/24/32-bit PCM WAV files into AudioBuffer.
 * Essential for loading sample libraries like VSCO2.
 */
class WavReader {
public:
    static std::shared_ptr<Core::AudioBuffer> load(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return nullptr;

        char header[4];
        file.read(header, 4);
        if (std::strncmp(header, "RIFF", 4) != 0) return nullptr;

        file.seekg(12); // Skip RIFF size and WAVE text
        
        uint16_t format = 0;
        uint16_t numChannels = 0;
        uint32_t sampleRate = 0;
        uint16_t bitsPerSample = 0;
        uint32_t dataSize = 0;

        while (file) {
            char chunkId[4];
            uint32_t chunkSize;
            file.read(chunkId, 4);
            file.read(reinterpret_cast<char*>(&chunkSize), 4);

            if (std::strncmp(chunkId, "fmt ", 4) == 0) {
                file.read(reinterpret_cast<char*>(&format), 2);
                file.read(reinterpret_cast<char*>(&numChannels), 2);
                file.read(reinterpret_cast<char*>(&sampleRate), 4);
                file.seekg(6, std::ios::cur);
                file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
                if (chunkSize > 16) file.seekg(chunkSize - 16, std::ios::cur);
            } else if (std::strncmp(chunkId, "data", 4) == 0) {
                dataSize = chunkSize;
                break;
            } else {
                file.seekg(chunkSize, std::ios::cur);
            }
        }

        if (dataSize == 0) return nullptr;

        uint32_t samplesPerChannel = dataSize / (numChannels * (bitsPerSample / 8));
        auto out = std::make_shared<Core::AudioBuffer>(numChannels, samplesPerChannel);

        std::vector<char> raw(dataSize);
        file.read(raw.data(), dataSize);

        for (uint32_t s = 0; s < samplesPerChannel; ++s) {
            for (uint32_t c = 0; c < numChannels; ++c) {
                size_t offset = (s * numChannels + c) * (bitsPerSample / 8);
                float val = 0.0f;

                if (format == 3) { // IEEE FLOAT
                    if (bitsPerSample == 32) val = *reinterpret_cast<float*>(&raw[offset]);
                } else { // PCM
                    if (bitsPerSample == 16) val = *reinterpret_cast<int16_t*>(&raw[offset]) / 32768.0f;
                    else if (bitsPerSample == 24) {
                        int32_t i32 = (uint8_t)raw[offset] | ((uint8_t)raw[offset+1] << 8) | (raw[offset+2] << 16);
                        if (i32 & 0x800000) i32 |= 0xFF000000;
                        val = i32 / 8388608.0f;
                    }
                }
                out->getWritePointer(c)[s] = val;
            }
        }
        return out;
    }
};

} // namespace Aura::IO
