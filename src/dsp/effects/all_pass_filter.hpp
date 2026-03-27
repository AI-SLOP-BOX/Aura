#pragma once

#include <vector>

namespace Aura::DSP::Effects {

/**
 * @brief AllPassFilter: Phase-shifting without amplitude change for diffusion.
 * Addresses the "low echo density" concern in the professional review.
 */
class AllPassFilter {
public:
    AllPassFilter(size_t delaySamples, float feedback) 
        : m_feedback(feedback) {
        uint32_t size = 1;
        while (size <= (uint32_t)delaySamples) size <<= 1;
        m_delayBuffer.resize(size, 0.0f);
        m_mask = size - 1;
    }

    float process(float in) {
        float bufferOut = m_delayBuffer[m_idx];
        float out = -m_feedback * in + bufferOut;
        m_delayBuffer[m_idx] = in + m_feedback * bufferOut;

        m_idx = (m_idx + 1) & m_mask;
        return out;
    }

private:
    std::vector<float> m_delayBuffer;
    float m_feedback;
    size_t m_idx = 0;
    uint32_t m_mask;
};

} // namespace Aura::DSP::Effects
