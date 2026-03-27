#pragma once

#include <cmath>
#include <atomic>

namespace Aura::Core::Engine {

/**
 * @brief LinearSmoother: Professional sample-accurate parameter interpolation.
 * Eliminates "zipper noise" by ramping values linearly across an audio block.
 */
class LinearSmoother {
public:
    explicit LinearSmoother(float initialValue = 0.0f) 
        : m_currentValue(initialValue), m_targetValue(initialValue), 
          m_stepValue(0.0f), m_remainingSamples(0), m_rampSize(512) {}

    /**
     * @brief Sets a new target value to be reached over a specific number of samples.
     * HONEST REFACTOR: Uses an explicit counter to prevent precision drift.
     */
    void setTarget(float target, uint32_t numSamples) {
        m_targetValue = target;
        m_remainingSamples = numSamples;
        if (numSamples > 0) {
            m_stepValue = (m_targetValue - m_currentValue) / static_cast<float>(numSamples);
        } else {
            m_currentValue = m_targetValue;
            m_stepValue = 0.0f;
        }
    }

    void setSmoothingTime(float ms, float sampleRate) {
        m_rampSize = static_cast<uint32_t>((ms * 0.001f) * sampleRate);
    }

    void setTarget(float target) {
        setTarget(target, m_rampSize);
    }

    /**
     * @brief Advances the smoother by one sample.
     * PERFORMANCE FIX: Replaced std::abs branch with a simple counter decrement.
     */
    inline float getNextValue() {
        if (m_remainingSamples > 0) {
            m_currentValue += m_stepValue;
            --m_remainingSamples;
            
            if (m_remainingSamples == 0) {
                m_currentValue = m_targetValue; // Snap to target on final sample
            }
        }
        return m_currentValue;
    }

    void reset(float value) {
        m_currentValue = value;
        m_targetValue = value;
        m_stepValue = 0.0f;
        m_remainingSamples = 0;
    }

    float getCurrentValue() const { return m_currentValue; }

private:
    float m_currentValue;
    float m_targetValue;
    float m_stepValue;
    uint32_t m_remainingSamples;
    uint32_t m_rampSize;
};

} // namespace Aura::Core::Engine
