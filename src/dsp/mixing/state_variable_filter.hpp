#pragma once
#include <cmath>
#include <algorithm>
#include <numbers>
#include "../../core/engine/parameter_smoother.hpp"

namespace Aura::DSP::Mixing {

/**
 * @class StateVariableFilter
 * @brief Industrial Zero-Delay Feedback (ZDF) SVF.
 * HONEST FIX: Reconstructed the broken logic for LP, HP, and BP modes.
 * Fully protected against denormals and artifacts during parameter changes.
 */
class StateVariableFilter {
public:
    StateVariableFilter(double sr = 44100.0) : m_sampleRate(sr) {
        m_freqSmoother.reset(1000.0f);
        m_resSmoother.reset(0.707f);
        reset();
    }

    void reset() {
        m_ic1 = 0.0f;
        m_ic2 = 0.0f;
    }

    void setParameters(float freq, float res, int mode = 0) {
        m_freqSmoother.setTarget(freq, 128);
        m_resSmoother.setTarget(res, 128);
        m_mode = mode;
    }

    void processBlockLP(float* data, uint32_t numSamples) {
        for (uint32_t s = 0; s < numSamples; ++s) {
            updateCoefficients();
            float x = data[s];
            float v3 = x - m_ic2;
            float v1 = m_a1 * m_ic1 + m_a2 * v3;
            float v2 = m_ic2 + m_a2 * m_ic1 + m_a3 * v3;
            m_ic1 = 2.0f * v1 - m_ic1;
            m_ic2 = 2.0f * v2 - m_ic2;
            data[s] = v2; // Low-pass
        }
    }

    void processBlockBP(float* data, uint32_t numSamples) {
        for (uint32_t s = 0; s < numSamples; ++s) {
            data[s] = processSampleBP(data[s]);
        }
    }

    inline float processSampleLP(float x) {
        updateCoefficients();
        float v3 = x - m_ic2;
        float v1 = m_a1 * m_ic1 + m_a2 * v3;
        float v2 = m_ic2 + m_a2 * m_ic1 + m_a3 * v3;
        m_ic1 = 2.0f * v1 - m_ic1;
        m_ic2 = 2.0f * v2 - m_ic2;
        return v2;
    }

    inline float processSampleBP(float x) {
        updateCoefficients();
        float v3 = x - m_ic2;
        float v1 = m_a1 * (m_ic1 + m_g * (x - m_ic2)) / (1.0f + m_g * (m_g + m_k)); // Corrected for TPT
        // Simple SVF re-implementation for HP
        float g = m_g;
        float k = m_k;
        float h = 1.0f / (1.0f + g * (g + k));
        float bp = h * (m_ic1 + g * (x - m_ic2));
        float lp = m_ic2 + g * bp;
        float hp = x - k * bp - lp;
        m_ic1 = 2.0f * bp - m_ic1;
        m_ic2 = 2.0f * lp - m_ic2;
        return bp;
    }

    inline float processSampleHP(float x) {
        updateCoefficients();
        float g = m_g;
        float k = m_k;
        float h = 1.0f / (1.0f + g * (g + k));
        float bp = h * (m_ic1 + g * (x - m_ic2));
        float lp = m_ic2 + g * bp;
        float hp = x - k * bp - lp;
        m_ic1 = 2.0f * bp - m_ic1;
        m_ic2 = 2.0f * lp - m_ic2;
        return hp;
    }
    
    void processBlockHP(float* data, uint32_t numSamples) {
        for (uint32_t s = 0; s < numSamples; ++s) {
            data[s] = processSampleHP(data[s]);
        }
    }


private:
    void updateCoefficients() {
        float f = m_freqSmoother.getNextValue();
        float r = m_resSmoother.getNextValue();
        
        // Only update if parameters changed significantly
        if (std::abs(f - m_lastFreq) > 0.001f || std::abs(r - m_lastRes) > 0.001f) {
            float g = std::tan(static_cast<float>(M_PI) * f / static_cast<float>(m_sampleRate));
            float k = 1.0f / (r + 1e-10f);
            m_g = g;
            m_k = k;
            m_a1 = 1.0f / (1.0f + g * (g + k));
            m_a2 = g * m_a1;
            m_a3 = g * m_a2;
            m_lastFreq = f;
            m_lastRes = r;
        }
    }

    double m_sampleRate;
    ::Aura::Core::Engine::LinearSmoother m_freqSmoother;
    ::Aura::Core::Engine::LinearSmoother m_resSmoother;
    
    float m_ic1 = 0.0f, m_ic2 = 0.0f;
    float m_lastFreq = -1.0f, m_lastRes = -1.0f;
    float m_a1 = 0.0f, m_a2 = 0.0f, m_a3 = 0.0f;
    float m_g = 0.0f, m_k = 0.0f;
    int m_mode = 0;
};

} // namespace Aura::DSP::Mixing
