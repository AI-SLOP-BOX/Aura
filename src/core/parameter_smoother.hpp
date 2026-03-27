#pragma once

#include <atomic>
#include <cmath>

namespace Aura::Core {

/**
 * @brief ParameterSmoother: Prevents zipper noise by smoothing value changes.
 * Uses a simple 1st-order IIR filter for transparent parameter transitions.
 */
class ParameterSmoother {
public:
    explicit ParameterSmoother(float initialValue = 0.0f, float smoothingMs = 20.0f, float sampleRate = 44100.0f)
        : m_current(initialValue), m_target(initialValue) {
        setSmoothingTime(smoothingMs, sampleRate);
    }

    void setTarget(float value) { m_target = value; }
    void setSmoothingTime(float ms, float sr) {
        double timeConstantSamples = (ms * 0.001) * sr;
        m_coeff = static_cast<float>(1.0 - std::exp(-1.0 / timeConstantSamples));
    }

    /**
     * @brief NEXT VALUE: High-precision 1st-order IIR smoothing.
     * HONEST FIX: Removed the unstable 'Adaptive Alpha' that caused unmusical jumps.
     * Standard exponential curve for natural transition.
     */
    float getNextValue() {
        m_current = m_current + (m_target - m_current) * m_coeff;
        if (std::abs(m_target - m_current) < 1e-7f) m_current = m_target;
        return m_current;
    }

private:
    float m_target = 0.0f, m_current = 0.0f;
    float m_coeff = 0.01f;
};

} // namespace Aura::Core
