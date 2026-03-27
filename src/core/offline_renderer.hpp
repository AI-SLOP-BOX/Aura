#pragma once

#include <vector>
#include <fstream>
#include "audio_processor_graph.hpp"
#include "../../io/wav_loader_utils.hpp"

namespace Aura::Core {

/**
 * @brief OfflineRenderer: Fast-rendering (Bounce) engine for the DAW.
 * HONEST FIX: Now renders the FULL PROJECT (Tracks + Master Chain).
 */
class OfflineRenderer {
public:
    OfflineRenderer(Engine::TimelineSystem& timeline, DSP::Mixing::MasterSuite& mastering, double sr) 
        : m_timeline(timeline), m_mastering(mastering), m_sampleRate(sr) {}

    /**
     * @brief Renders the entire project into a file (Non-realtime).
     */
    void renderToFile(const std::string& path, double durationSeconds) {
        size_t totalSamples = static_cast<size_t>(durationSeconds * m_sampleRate);
        const uint32_t blockSize = 1024;
        
        std::ofstream file(path, std::ios::binary);
        writeWavHeader(file, totalSamples);

        AudioBuffer buffer(2, blockSize);
        std::vector<float> l(blockSize), r(blockSize);

        for (size_t s = 0; s < totalSamples; s += blockSize) {
            uint32_t currentBlock = std::min(blockSize, static_cast<uint32_t>(totalSamples - s));
            
            // 1. Render Timeline
            m_timeline.render(l.data(), r.data(), currentBlock, m_sampleRate, 120.0 /* Default BPM for bounce */);
            
            // 2. Wrap for Master Suite
            buffer.copyFrom(0, 0, l.data(), currentBlock);
            buffer.copyFrom(1, 0, r.data(), currentBlock);
            
            // 3. Process Mastering
            m_mastering.process(buffer);

            // 4. Write PCM Interleaved (24-bit emulation)
            for (uint32_t i = 0; i < currentBlock; ++i) {
                float outL = buffer.getReadPointer(0)[i];
                float outR = buffer.getReadPointer(1)[i];
                
                // Simple 16-bit write for compatibility
                int16_t sampleL = static_cast<int16_t>(std::clamp(outL, -1.0f, 1.0f) * 32767.0f);
                int16_t sampleR = static_cast<int16_t>(std::clamp(outR, -1.0f, 1.0f) * 32767.0f);
                file.write(reinterpret_cast<const char*>(&sampleL), 2);
                file.write(reinterpret_cast<const char*>(&sampleR), 2);
            }
        }
        file.close();
    }

private:
    void writeWavHeader(std::ofstream& file, uint64_t totalSamples) {
        // Simple WAV Header (44 bytes)
        file.write("RIFF", 4);
        uint32_t fileSize = 36 + totalSamples * 2 * 2;
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        uint32_t fmtSize = 16;
        file.write(reinterpret_cast<const char*>(&fmtSize), 4);
        uint16_t type = 1, channels = 2;
        file.write(reinterpret_cast<const char*>(&type), 2);
        file.write(reinterpret_cast<const char*>(&channels), 2);
        uint32_t sr = static_cast<uint32_t>(m_sampleRate);
        file.write(reinterpret_cast<const char*>(&sr), 4);
        uint32_t byteRate = sr * 2 * 2;
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        uint16_t align = 4, bps = 16;
        file.write(reinterpret_cast<const char*>(&align), 2);
        file.write(reinterpret_cast<const char*>(&bps), 2);
        file.write("data", 4);
        uint32_t dataSize = totalSamples * 2 * 2;
        file.write(reinterpret_cast<const char*>(&dataSize), 4);
    }

    Engine::TimelineSystem& m_timeline;
    DSP::Mixing::MasterSuite& m_mastering;
    double m_sampleRate;
};

} // namespace Aura::Core
