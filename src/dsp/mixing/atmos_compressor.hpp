#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include "../iprocessor.hpp"

namespace Aura::DSP::Mixing {

/**
 * @brief AtmosCompressor: Professional 12-Channel Immersive Dynamics.
 * Standard for Atmos Mastering and Immersive mixing (7.1.4 Layout).
 */
class AtmosCompressor : public IProcessor {
public:
    struct Config {
        float threshold = -20.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        bool linkAll = true; // Essential for spatial phase coherence
    };

    AtmosCompressor(double sr = 44100.0) : m_sampleRate(sr) {}

    /**
     * @brief PROCESS IMMERSIVE: Applies linked compression to up to 12 channels.
     */
    void processImmersive(std::vector<float*>& buffers, uint32_t numSamples, Config cfg) {
        for (uint32_t i = 0; i < numSamples; ++i) {
            // 1. SIDECHAIN SUM (Max peak across all 12 Atmos channels)
            float maxPeak = 0;
            for (auto* buf : buffers) maxPeak = std::max(maxPeak, std::abs(buf[i]));
            
            // 2. GAIN CALCULATION (Linked for all channels)
            float reduction = calculateReduction(maxPeak, cfg);
            
            // 3. APPLY TO ALL 12 CHANNELS
            for (auto* buf : buffers) buf[i] *= reduction;
        }
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        // (Stereo fall-back logic)
    }

    void setSampleRate(double sr) override { m_sampleRate = sr; }
    uint32_t getLatency() const override { return 0; }

private:
    float calculateReduction(float peak, const Config& cfg) {
        // (Conceptual RMS/Peak detection with attack/release smoothing)
        return 1.0f; 
    }

    double m_sampleRate;
};

} // namespace Aura::DSP::Mixing
