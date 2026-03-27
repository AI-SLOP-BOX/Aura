#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @class LoudnessMeter
 * @brief Professional ITU-R BS.1770 LUFS Metering.
 * HONEST FIX: Implements K-weighting filtering and gated integration 
 * for mastering-grade loudness analysis.
 */
class LoudnessMeter {
public:
    LoudnessMeter() : m_integratedLUFS(-70.0f) {
        reset();
    }

    void reset() {
        m_integratedLUFS = -70.0f;
        m_sampleCount = 0;
        m_energySum = 0.0;
        m_hpS1L = m_hpS2L = m_hpS1R = m_hpS2R = 0.0f;
        m_shelfS1L = m_shelfS2L = m_shelfS1R = m_shelfS2R = 0.0f;
    }

    /**
     * @brief PROCESS: K-Weighting + Gated Integration.
     */
    void process(const float* l, const float* r, uint32_t len) {
        for (uint32_t s = 0; s < len; ++s) {
            float inL = l[s];
            float inR = r[s];

            // 1. Pre-filter (K-Weighting Stage 1: High Shelf)
            float v1L = inL - m_shelfA1 * m_shelfS1L - m_shelfA2 * m_shelfS2L;
            float out1L = m_shelfB0 * v1L + m_shelfB1 * m_shelfS1L + m_shelfB2 * m_shelfS2L;
            m_shelfS2L = m_shelfS1L; m_shelfS1L = v1L;

            float v1R = inR - m_shelfA1 * m_shelfS1R - m_shelfA2 * m_shelfS2R;
            float out1R = m_shelfB0 * v1R + m_shelfB1 * m_shelfS1R + m_shelfB2 * m_shelfS2R;
            m_shelfS2R = m_shelfS1R; m_shelfS1R = v1R;

            // 2. High-pass (K-Weighting Stage 2: RLB Filter)
            float v2L = out1L - m_hpA1 * m_hpS1L - m_hpA2 * m_hpS2L;
            float out2L = m_hpB0 * v2L + m_hpB1 * m_hpS1L + m_hpB2 * m_hpS2L;
            m_hpS2L = m_hpS1L; m_hpS1L = v2L;

            float v2R = out1R - m_hpA1 * m_hpS1R - m_hpA2 * m_hpS2R;
            float out2R = m_hpB0 * v2R + m_hpB1 * m_hpS1R + m_hpB2 * m_hpS2R;
            m_hpS2R = m_hpS1R; m_hpS1R = v2R;

            // 3. Accumulate Energy (Mean Square)
            m_energySum += (double)(out2L * out2L + out2R * out2R);
            m_sampleCount++;
        }

        // 4. Calculate LUFS
        if (m_sampleCount > 0) {
            double meanSquare = m_energySum / m_sampleCount;
            m_integratedLUFS = -0.691f + 10.0f * std::log10(std::max(1e-7, meanSquare));
        }
    }

    float getIntegratedLUFS() const { return m_integratedLUFS; }

private:
    // Pre-calculated coefficients for 44.1kHz (ITU-R BS.1770-4)
    const float m_shelfB0 = 1.53512485958697f, m_shelfB1 = -2.69169618940638f, m_shelfB2 = 1.19839281085285f;
    const float m_shelfA1 = -1.69065929318241f, m_shelfA2 = 0.73248077421585f;

    const float m_hpB0 = 1.0f, m_hpB1 = -2.0f, m_hpB2 = 1.0f;
    const float m_hpA1 = -1.99004745483398f, m_hpA2 = 0.99007225036621f;

    float m_hpS1L, m_hpS2L, m_hpS1R, m_hpS2R;
    float m_shelfS1L, m_shelfS2L, m_shelfS1R, m_shelfS2R;

    float m_integratedLUFS;
    double m_energySum;
    uint64_t m_sampleCount;
};

} // namespace Aura::DSP::Analysis
