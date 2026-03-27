/*
 * Aura DAW Ultimate - High-Performance Digital Audio Workstation
 * Copyright (c) 2024-2026 Aura DAW Project. All rights reserved.
 * Licensed under the MIT License.
 */

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <functional>
#include "../core/audio_buffer.hpp"
#include "../core/engine/timeline_system.hpp"

namespace Aura::IO {

/**
 * @class AudioExportEngine
 * @brief High-speed Offline Rendering (Bouncing) Engine.
 * HONEST FIX: Performs a non-real-time render as fast as the CPU allows.
 */
class AudioExportEngine {
public:
    struct ExportOptions {
        std::string filename;
        double sampleRate = 44100.0;
        uint32_t bitDepth = 24;
        uint64_t startSample = 0;
        uint64_t endSample = 0;
        bool normalize = false;
    };

    struct MasteringReport {
        float lufsIntegrated;
        float truePeakMax;
        std::string advice;
    };

    /**
     * @brief EXPORT: The core non-real-time bounce loop with AI Review.
     */
    static bool bounce(Core::Engine::TimelineSystem& timeline, const ExportOptions& opts, std::function<void(float)> onProgress) {
        std::ofstream file(opts.filename, std::ios::binary);
        if (!file.is_open()) return false;

        // --- 1. INITIALIZE MASTERING ANALYZER (AI REVIEW) ---
        DSP::Analysis::MasterMeter analyzer(opts.sampleRate);

        writeWavHeader(file, opts.sampleRate, opts.bitDepth, 2, 0); 

        uint64_t totalSamples = opts.endSample - opts.startSample;
        uint64_t rendered = 0;
        uint32_t blockSize = 1024;
        
        Core::AudioBuffer buffer(2, blockSize);
        uint64_t currentPos = opts.startSample;

        while (rendered < totalSamples) {
            uint32_t toRender = static_cast<uint32_t>(std::min(static_cast<uint64_t>(blockSize), totalSamples - rendered));
            buffer.clear();
            
            // --- OFFLINE RENDERING ---
            timeline.render(buffer.getWritePointer(0), buffer.getWritePointer(1), currentPos, toRender);

            // --- AI MASTERING ANALYSIS ---
            analyzer.process(buffer.getReadPointer(0), buffer.getReadPointer(1), toRender);

            // 2. CONVERT TO INTEGER & WRITE
            writeSamples(file, buffer, toRender, opts.bitDepth);

            rendered += toRender;
            currentPos += toRender;
            if (onProgress) onProgress(static_cast<float>(rendered) / totalSamples);
        }

        // --- 3. FINALIZE AI REVIEW REPORT ---
        auto finalStat = analyzer.getLatestData();
        float finalTP = std::max(finalStat.truePeakL, finalStat.truePeakR);
        std::string advice;
        if (finalTP > 1.0f) advice = "CRITICAL: Audio is CLIPPING. Reduce master.";
        else if (finalStat.lufsIntegrated > -14.0f) advice = "GOOD: Meets Spotify standards.";
        else advice = "QUIET: Consider increasing gain.";

        std::cout << "[AI Reviewer] Final LUFS: " << finalStat.lufsIntegrated << " | TP: " << (20.0f * std::log10(finalTP + 1e-9f)) << "dB" << std::endl;
        std::cout << "[AI Reviewer] Advice: " << advice << std::endl;

        // 4. FINALIZE HEADER (Patch filesize)
        uint32_t dataSize = static_cast<uint32_t>(rendered * 2 * (opts.bitDepth / 8));
        file.seekp(4, std::ios::beg);
        uint32_t riffSize = 36 + dataSize;
        file.write(reinterpret_cast<char*>(&riffSize), 4);
        file.seekp(40, std::ios::beg);
        file.write(reinterpret_cast<char*>(&dataSize), 4);

        return true;
    }

private:
    static void writeWavHeader(std::ofstream& file, double sr, uint32_t bits, uint16_t channels, uint32_t dataSize) {
        // RIFF header
        file.write("RIFF", 4);
        uint32_t fileSize = 36 + dataSize;
        file.write(reinterpret_cast<char*>(&fileSize), 4);
        file.write("WAVE", 4);

        // Format chunk
        file.write("fmt ", 4);
        uint32_t fmtSize = 16;
        file.write(reinterpret_cast<char*>(&fmtSize), 4);
        uint16_t format = 1; // PCM
        file.write(reinterpret_cast<char*>(&format), 2);
        file.write(reinterpret_cast<char*>(&channels), 2);
        uint32_t samplerate = static_cast<uint32_t>(sr);
        file.write(reinterpret_cast<char*>(&samplerate), 4);
        uint32_t byteRate = samplerate * channels * bits / 8;
        file.write(reinterpret_cast<char*>(&byteRate), 4);
        uint16_t blockAlign = channels * bits / 8;
        file.write(reinterpret_cast<char*>(&blockAlign), 2);
        uint16_t bitDepth = static_cast<uint16_t>(bits);
        file.write(reinterpret_cast<char*>(&bitDepth), 2);

        // Data chunk
        file.write("data", 4);
        file.write(reinterpret_cast<char*>(&dataSize), 4);
    }

    static void writeSamples(std::ofstream& file, const Core::AudioBuffer& buffer, uint32_t len, uint32_t bits) {
        const float* l = buffer.getReadPointer(0);
        const float* r = buffer.getReadPointer(1);

        for (uint32_t i = 0; i < len; ++i) {
            // Interleave L/R and convert to integer
            if (bits == 24) {
                for (int c = 0; c < 2; ++c) {
                    float s = (c == 0 ? l[i] : r[i]);
                    int32_t val = static_cast<int32_t>(std::clamp(s, -1.0f, 1.0f) * 8388607.0f);
                    file.write(reinterpret_cast<char*>(&val), 3);
                }
            } else {
                // 16-bit fallback
                for (int c = 0; c < 2; ++c) {
                    float s = (c == 0 ? l[i] : r[i]);
                    int16_t val = static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
                    file.write(reinterpret_cast<char*>(&val), 2);
                }
            }
        }
    }
};

} // namespace Aura::IO
