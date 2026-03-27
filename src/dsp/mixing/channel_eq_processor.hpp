#pragma once

#include <vector>
#include <cmath>
#include <numbers>

namespace Aura::Core::DSP::Mixing {

/**
 * @brief High-Fidelity Biquad Filter: Professional RBJ implementation.
 */
class BiquadFilter {
public:
    void calculatePeaking(float freq, float sr, float Q, float gainDB) {
        float A = std::pow(10.0f, gainDB / 40.0f);
        float omega = 2.0f * std::numbers::pi_v<float> * freq / sr;
        float alpha = std::sin(omega) / (2.0f * Q);

        m_b0 = 1.0f + alpha * A;
        m_b1 = -2.0f * std::cos(omega);
        m_b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        m_a1 = -2.0f * std::cos(omega);
        m_a2 = 1.0f - alpha / A;

        // Normalizing coefficients to a0
        m_b0 /= a0; m_b1 /= a0; m_b2 /= a0;
        m_a1 /= a0; m_a2 /= a0;
    }

    float process(float x) {
        float y = m_b0 * x + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1; m_x1 = x;
        m_y2 = m_y1; m_y1 = y;
        return y;
    }

private:
    float m_b0, m_b1, m_b2, m_a1, m_a2;
    float m_x1 = 0, m_x2 = 0, m_y1 = 0, m_y2 = 0;
};

/**
 * @brief ChannelEQProcessor: Refactored with high-fidelity RBJ filters.
 */
class ChannelEQProcessor {
public:
    ChannelEQProcessor(float sr) {
        m_filters[0].calculatePeaking(100.0f, sr, 0.707f, 0.0f);
        m_filters[1].calculatePeaking(1000.0f, sr, 0.707f, 0.0f);
        m_filters[2].calculatePeaking(5000.0f, sr, 0.707f, 0.0f);
    }

    void process(float* l, float* r, size_t numFrames) {
        for (size_t i = 0; i < numFrames; ++i) {
            l[i] = m_filters[0].process(l[i]);
            r[i] = m_filters[0].process(r[i]);
            // Apply other bands...
        }
    }

private:
    std::vector<BiquadFilter> m_filters{3};
};

} // namespace Aura::Core::DSP::Mixing
