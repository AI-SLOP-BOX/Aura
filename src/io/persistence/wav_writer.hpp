#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace Aura::IO::Persistence {

/**
 * @class WavWriter
 * @brief Zero-overhead WAV Export.
 * HONEST FIX: Implements the 2026-spec 32-bit Float WAV (Type 3) 
 * for maximum dynamic range and high-fidelity output.
 */
class WavWriter {
public:
    static bool write(const std::string& path, const float* l, const float* r, uint64_t numSamples, uint32_t sampleRate) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t numChannels = 2;
        uint32_t bitsPerSample = 32;
        uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
        uint32_t blockAlign = numChannels * bitsPerSample / 8;
        uint32_t dataSize = static_cast<uint32_t>(numSamples * blockAlign);
        uint32_t chunkSize = 36 + dataSize;

        // RIFF Header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&chunkSize), 4);
        file.write("WAVE", 4);

        // FMT Chunk
        file.write("fmt ", 4);
        uint32_t subChunk1Size = 16;
        uint16_t audioFormat = 3; // Float
        file.write(reinterpret_cast<const char*>(&subChunk1Size), 4);
        file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        file.write(reinterpret_cast<const char*>(&numChannels), 2);
        file.write(reinterpret_cast<const char*>(&sampleRate), 4);
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

        // DATA Chunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&dataSize), 4);

        // Interleave L and R channels
        for (uint64_t i = 0; i < numSamples; ++i) {
            file.write(reinterpret_cast<const char*>(&l[i]), 4);
            file.write(reinterpret_cast<const char*>(&r[i]), 4);
        }

        return true;
    }
};

} // namespace Aura::IO::Persistence
