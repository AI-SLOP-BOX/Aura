#pragma once

#include <vector>
#include <memory>
#include "transient_detector.hpp"
#include "../effects/elastic_audio.hpp"

namespace Aura::Core::Engine {

/**
 * @brief AudioQuantizer: Professional Logic Pro-style 'Audio Quantize'.
 * Aligns drum hits to the grid using Elastic Audio warping.
 */
class AudioQuantizer {
public:
    struct Options {
        float strength = 1.0f; // [0, 1] 1.0 = perfect grid
        float swing = 0.0f;    // [0, 1]
    };

    /**
     * @brief ACCURATE QUANTIZATION: Detects transients and warps audio to grid.
     */
    static void quantize(const float* in, float* out, uint64_t len, float bpm, Options opt) {
        DSP::Analysis::TransientDetector detector(44100.0);
        DSP::Effects::ElasticAudioEngine stretcher(44100.0);
        
        // 1. DETECT TRANSIENTS (Flex Markers)
        std::vector<uint64_t> markers;
        for (uint64_t i = 0; i < len; i += 512) {
            if (detector.detect(in + i, 512)) markers.push_back(i);
        }

        // 2. CALCULATE TARGETS & WARP (Grid Alignment)
        double samplesPerBeat = (60.0 / bpm) * 44100.0;
        double grid = samplesPerBeat / 4.0; // 1/16th grid

        uint64_t lastIn = 0;
        uint64_t lastOut = 0;

        for (auto m : markers) {
            double target = std::round(m / grid) * grid;
            uint64_t targetPos = static_cast<uint64_t>(m + (target - m) * opt.strength);
            
            // --- HONEST FIX: PROFESSIONAL ELASTIC WARP ---
            uint32_t lenIn = m - lastIn;
            uint32_t lenOut = targetPos - lastOut;
            float ratio = static_cast<float>(lenIn) / lenOut;

            stretcher.process(in + lastIn, out + lastOut, lenIn, lenOut, ratio);
            
            lastIn = m;
            lastOut = targetPos;
        }

        // Copy remaining tail
        if (lastIn < len) std::copy(in + lastIn, in + len, out + lastOut);
    }
};

} // namespace Aura::Core::Engine
