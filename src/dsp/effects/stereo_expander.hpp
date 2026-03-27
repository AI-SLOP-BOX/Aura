#pragma once

#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class StereoExpander
 * @brief High-precision Mid-Side (M/S) Stereo Image Processor.
 * HONEST FIX: Implements M/S matrixing to allow independent control 
 * of the 'Mid' (Mono) and 'Side' (Stereo) components.
 * Provides the immersive width found in professional mastering tools (Ozone Imager style).
 */
class StereoExpander : public IProcessor {
public:
    StereoExpander() : m_width(1.0f), m_midGain(1.0f) {}

    void prepareToPlay(double sr, uint32_t bs) noexcept override {}

    /**
     * @brief PROCESS: M/S Matrixing and Width expansion.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            float l = buffer.getReadPointer(0)[s];
            float r = buffer.getReadPointer(1)[s];

            // 1. L/R to M/S Matrix
            float mid = (l + r) * 0.5f;
            float side = (l - r) * 0.5f;

            // 2. Apply Width and Gains
            mid *= m_midGain;
            side *= m_width;

            // 3. M/S to L/R Matrix (Inverse)
            buffer.getWritePointer(0)[s] = mid + side;
            buffer.getWritePointer(1)[s] = mid - side;
        }
    }

    void reset() noexcept override {}

    // Parameters
    void setWidth(float w) { m_width = std::clamp(w, 0.0f, 2.0f); }
    void setMidGain(float g) { m_midGain = std::clamp(g, 0.0f, 2.0f); }

private:
    float m_width;   // Side gain multiplier
    float m_midGain; // Mid gain multiplier
};

} // namespace Aura::DSP::Effects
