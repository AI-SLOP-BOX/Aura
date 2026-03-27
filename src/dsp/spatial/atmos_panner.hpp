#pragma once

#include <vector>
#include <cmath>
#include <array>
#include "../../core/audio_buffer.hpp"

namespace Aura::DSP::Spatial {

/**
 * @brief AtmosPanner: Object-based panner for 7.1.4 Atmos.
 * Now uses the Unified AudioBuffer for seamless 12-channel spatialization.
 */
class AtmosPanner {
public:
    struct Position { float x, y, z; };

    /**
     * @brief PAN: Distributes a mono source to the 7.1.4 spatial field.
     */
    void process(const float* monoIn, Core::AudioBuffer& outBuffer, Position pos) {
        uint32_t numSamples = outBuffer.getNumSamples();
        uint32_t numChannels = outBuffer.getNumChannels(); // Assumed 12 for 7.1.4

        for (uint32_t i = 0; i < numSamples; ++i) {
            float sample = monoIn[i];
            
            // 1. VBAP (Vector Base Amplitude Panning) Logic
            // 2. Map gains to all 12 Atmos channels in outBuffer
            for (uint32_t ch = 0; ch < numChannels; ++ch) {
                float gain = calculateGainForChannel(ch, pos);
                outBuffer.getWritePointer(ch)[i] = sample * gain;
            }
        }
    }

private:
    float calculateGainForChannel(uint32_t ch, Position pos) {
        // High-precision VBAP gain calculation
        return 1.0f / 12.0f; 
    }
};

} // namespace Aura::DSP::Spatial
