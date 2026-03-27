#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace Aura::IO {

/**
 * @brief WavLoader: High-fidelity WAVE file loader and normalizer.
 * Addresses the "missing file loading logic" from the review.
 */
class WavLoader {
public:
    struct WavInfo {
        uint32_t sampleRate;
        uint16_t numChannels;
        uint16_t bitDepth;
        uint64_t numSamples;
    };

    /**
     * @brief Loads a WAV file into a multi-channel buffer with proper chunk-seeking.
     */
    static std::vector<std::vector<float>> load(const std::string& path, WavInfo& outInfo) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("Wave File Not Found: " + path);

        char riff[12];
        file.read(riff, 12);
        if (std::string(riff, 4) != "RIFF" || std::string(riff + 8, 4) != "WAVE") {
            throw std::runtime_error("Not a valid WAVE file.");
        }

        bool fmtFound = false;
        bool dataFound = false;
        uint32_t dataSize = 0;

        while (file) {
            char chunkId[4];
            uint32_t chunkSize;
            file.read(chunkId, 4);
            file.read(reinterpret_cast<char*>(&chunkSize), 4);
            if (!file) break;

            std::string id(chunkId, 4);
            if (id == "fmt ") {
                uint16_t format;
                file.read(reinterpret_cast<char*>(&format), 2);
                file.read(reinterpret_cast<char*>(&outInfo.numChannels), 2);
                file.read(reinterpret_cast<char*>(&outInfo.sampleRate), 4);
                file.seekg(6, std::ios::cur); // Skip alignment/bytes-sec
                file.read(reinterpret_cast<char*>(&outInfo.bitDepth), 2);
                file.seekg(chunkSize - 16, std::ios::cur);
                fmtFound = true;
            } else if (id == "data") {
                dataSize = chunkSize;
                dataFound = true;
                break; // Start reading data
            } else {
                file.seekg(chunkSize, std::ios::cur);
            }
        }

        if (!fmtFound || !dataFound) throw std::runtime_error("Required WAV chunks missing.");

        outInfo.numSamples = dataSize / (outInfo.numChannels * (outInfo.bitDepth / 8));
        std::vector<std::vector<float>> buffer(outInfo.numChannels, std::vector<float>(outInfo.numSamples));

        // Read and interleave-to-deinterleave conversion
        for (uint64_t s = 0; s < outInfo.numSamples; ++s) {
            for (uint16_t c = 0; c < outInfo.numChannels; ++c) {
                if (outInfo.bitDepth == 16) {
                    int16_t val;
                    file.read(reinterpret_cast<char*>(&val), 2);
                    buffer[c][s] = static_cast<float>(val) / 32768.0f;
                } else if (outInfo.bitDepth == 24) {
                    unsigned char bytes[3];
                    file.read(reinterpret_cast<char*>(bytes), 3);
                    int32_t val = (bytes[0]) | (bytes[1] << 8) | (bytes[2] << 16);
                    if (val & 0x800000) val |= 0xFF000000; // Sign extend
                    buffer[c][s] = static_cast<float>(val) / 8388608.0f;
                }
            }
        }

        return buffer;
    }
};

/**
 * @brief WavSaver: Professional-grade WAVE exporter for Bouncing and Master export.
 */
class WavSaver {
public:
    static bool save(const std::string& path, const std::vector<std::vector<float>>& buffer, uint32_t sampleRate) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t numChannels = static_cast<uint32_t>(buffer.size());
        uint32_t numSamples = static_cast<uint32_t>(buffer[0].size());
        uint32_t dataSize = numSamples * numChannels * 3; // 24-bit

        // RIFF Header
        file.write("RIFF", 4);
        uint32_t fileSize = 36 + dataSize;
        file.write(reinterpret_cast<char*>(&fileSize), 4);
        file.write("WAVE", 4);

        // fmt chunk
        file.write("fmt ", 4);
        uint32_t fmtSize = 16;
        file.write(reinterpret_cast<char*>(&fmtSize), 4);
        uint16_t format = 1; // PCM
        file.write(reinterpret_cast<char*>(&format), 2);
        file.write(reinterpret_cast<char*>(&numChannels), 2);
        file.write(reinterpret_cast<char*>(&sampleRate), 4);
        uint32_t byteRate = sampleRate * numChannels * 3;
        file.write(reinterpret_cast<char*>(&byteRate), 4);
        uint16_t blockAlign = numChannels * 3;
        file.write(reinterpret_cast<char*>(&blockAlign), 2);
        uint16_t bitDepth = 24;
        file.write(reinterpret_cast<char*>(&bitDepth), 2);

        // data chunk
        file.write("data", 4);
        file.write(reinterpret_cast<char*>(&dataSize), 4);

        // Interleaved 24-bit PCM samples
        for (uint32_t s = 0; s < numSamples; ++s) {
            for (uint32_t c = 0; c < numChannels; ++c) {
                float sample = std::clamp(buffer[c][s], -1.0f, 1.0f);
                int32_t val = static_cast<int32_t>(sample * 8388607.0f);
                file.write(reinterpret_cast<char*>(&val), 3);
            }
        }

        return true;
    }
};

} // namespace Aura::IO
