#pragma once

#include <atomic>
#include <cmath>
#include "../dsp/utils/dsp_math_utils.hpp"

namespace Aura::Core {

/**
 * @brief LinearSmoothedValue: Eliminates zipper noise by ramping values.
 * Addresses the "missing parameter smoothing" in the review.
 */
class LinearSmoothedValue {
public:
    LinearSmoothedValue(float initial, double sr) : m_current(initial), m_target(initial), m_sampleRate(sr) {
        setSmoothingTime(20.0f); // Default 20ms
    }

    void setTarget(float value) { m_target = value; }

    void setSmoothingTime(float ms) {
        m_ms = ms;
        m_step = (m_target - m_current) / (m_ms * 0.001f * static_cast<float>(m_sampleRate));
    }

    /**
     * @brief Gets the next smoothed sample value.
     */
    float next() {
        if (std::abs(m_target - m_current) < 1e-6f) return m_target;
        m_current += (m_target - m_current) * m_alpha;
        return m_current;
    }

    void updateSR(double sr) {
        m_sampleRate = sr;
        m_alpha = DSP::Utils::DSPMathUtils::calculateSmoothingAlpha(m_ms, m_sampleRate);
    }

private:
    float m_current;
    float m_target;
    float m_ms = 20.0f;
    float m_alpha = 0.01f;
    double m_sampleRate;
};

} // namespace Aura::Core
