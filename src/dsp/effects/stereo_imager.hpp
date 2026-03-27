#pragma once

#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../math/fast_math.hpp"

namespace Aura::DSP::Effects {

/**
 * @brief StereoImager: Advanced Mid-Side spatial processor.
 * Controls the width and spatial distribution of the stereo field.
 */
class StereoImager : public IProcessor {
public:
    StereoImager(double sr = 44100.0) : m_sampleRate(sr) {
        setWidth(1.0f); // Neutral width
    }

    /**
     * @brief Sets the stereo width factor.
     * @param width: 0.0 (Mono), 1.0 (Neutral), > 1.0 (Widened).
     */
    void setWidth(float width) {
        m_width = std::clamp(width, 0.0f, 4.0f);
        m_targetSideGain = m_width;
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        for (uint32_t i = 0; i < numSamples; ++i) {
            float inL = l[i];
            float inR = r[i];

            // 1. MID-SIDE ENCODING
            float mid = (inL + inR) * 0.5f;
            float side = (inL - inR) * 0.5f;

            // 2. SPATIAL SCULPTING (Width Control)
            // Note: Smooth gain change to prevent clicks
            m_currentSideGain += (m_targetSideGain - m_currentSideGain) * 0.01f;
            side *= m_currentSideGain;

            // 3. MID-SIDE DECODING (Back to L/R)
            // Compensation: Boost Mid slightly if Side is very wide to keep perceived power (optional)
            float comp = 1.0f / std::max(1.0f, m_currentSideGain * 0.5f);
            
            l[i] = (mid + side) * comp;
            r[i] = (mid - side) * comp;
        }
    }

    void setSampleRate(double sr) override { m_sampleRate = sr; }
    uint32_t getLatency() const override { return 0; }

private:
    double m_sampleRate;
    float m_width = 1.0f;
    float m_targetSideGain = 1.0f, m_currentSideGain = 1.0f;
    Math::FastMath m_math;
};

} // namespace Aura::DSP::Effects
