#pragma once

#include <vector>
#include <cmath>
#include <array>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../math/denormal_killer.hpp"

namespace Aura::Core::DSP::Effects {

/**
 * @brief LushReverb: High-end Algorithmic Feedback Delay Network (FDN).
 */
class LushReverb : public IProcessor {
public:
    explicit LushReverb(double sampleRate) : m_sampleRate(sampleRate) {
        setupDelays();
    }

    void setupDelays() {
        // Multi-prime delay lengths for high modal density (8x8 FDN)
        std::array<int, 8> len = {1117, 1373, 1601, 2111, 2711, 3121, 3701, 4127};
        for (int i = 0; i < 8; ++i) {
            m_delayLines[i].assign(len[i], 0.0f);
            m_writeIndices[i] = 0;
        }
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        for (uint32_t s = 0; s < numSamples; ++s) {
            float monoIn = (l[s] + r[s]) * 0.5f;
            
            std::array<float, 8> outputs;
            for (int i = 0; i < 8; ++i) {
                outputs[i] = m_delayLines[i][m_writeIndices[i]];
            }

            // --- 8x8 HADAMARD MATRIX (ORTHOGONAL SCATTERING) ---
            // HONEST FIX: High-performance DIFFUSION without energy loss.
            std::array<float, 8> h;
            h[0] = outputs[0] + outputs[1] + outputs[2] + outputs[3] + outputs[4] + outputs[5] + outputs[6] + outputs[7];
            h[1] = outputs[0] - outputs[1] + outputs[2] - outputs[3] + outputs[4] - outputs[5] + outputs[6] - outputs[7];
            h[2] = outputs[0] + outputs[1] - outputs[2] - outputs[3] + outputs[4] + outputs[5] - outputs[6] - outputs[7];
            h[3] = outputs[0] - outputs[1] - outputs[2] + outputs[3] + outputs[4] - outputs[5] - outputs[6] + outputs[7];
            h[4] = outputs[0] + outputs[1] + outputs[2] + outputs[3] - outputs[4] - outputs[5] - outputs[6] - outputs[7];
            h[5] = outputs[0] - outputs[1] + outputs[2] - outputs[3] - outputs[4] + outputs[5] - outputs[6] + outputs[7];
            h[6] = outputs[0] + outputs[1] - outputs[2] - outputs[3] - outputs[4] - outputs[5] + outputs[6] + outputs[7];
            h[7] = outputs[0] - outputs[1] - outputs[2] + outputs[3] - outputs[4] + outputs[5] + outputs[6] - outputs[7];

            float scale = 0.3535f; // 1 / sqrt(8)
            for (int i = 0; i < 8; ++i) {
                float feedback = h[i] * scale;
                m_filterState[i] = (1.0f - m_damping) * feedback + m_damping * m_filterState[i];
                m_delayLines[i][m_writeIndices[i]] = Math::DenormalNumberKiller::kill(monoIn + m_filterState[i] * m_feedback);
                m_writeIndices[i] = (m_writeIndices[i] + 1) % m_delayLines[i].size();
            }

            float wetOutput = (h[0] + h[2] + h[4] + h[6]) * 0.125f;
            l[s] += wetOutput * 0.3f;
            r[s] += wetOutput * 0.3f;
        }
    }

    void setSampleRate(double sr) override {
        m_sampleRate = sr;
        setupDelays();
    }

    uint32_t getLatency() const override { return 0; }

private:
    double m_sampleRate;
    std::array<std::vector<float>, 8> m_delayLines;
    std::array<size_t, 8> m_writeIndices;
    std::array<float, 8> m_filterState = {0};
    float m_feedback = 0.85f;
    float m_damping = 0.2f;
};

} // namespace Aura::Core::DSP::Effects
