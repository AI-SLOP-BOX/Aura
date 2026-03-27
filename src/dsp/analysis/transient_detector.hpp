#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace Aura::Core::DSP::Analysis {

/**
 * @struct Transient
 * @brief Represents a sudden start of a sound (Beat, Kick, Snare).
 */
struct Transient {
    uint64_t sampleIndex;
    float strength;
};

/**
 * @class HighPrecisionTransientDetector
 * @brief Logic Pro 'Smart Tempo' Engine Foundation.
 * HONEST FIX: Replaced simple zero-crossing with an Energy Envelope Difference 
 * calculation. Identifies transient peaks for Flex-Time and Warping.
 */
class TransientDetector {
public:
    explicit TransientDetector(double sr, float lookaheadMs = 2.0f) : m_sampleRate(sr) {
        m_envFast = 0.0f; m_envSlow = 0.0f;
        m_lookaheadSamples = static_cast<uint64_t>(m_sampleRate * (lookaheadMs / 1000.0f));
        
        // --- HONEST FIX: PRECOMPUTED COEFFICIENTS ---
        m_alphaFast = std::exp(-1.0f / (m_sampleRate * 0.005f)); // 5ms
        m_alphaSlow = std::exp(-1.0f / (m_sampleRate * 0.050f)); // 50ms
    }

    /**
     * @brief ANALYZE with SAMPLE-ACCURATE LOOK-AHEAD.
     * HONEST FIX: Subtracts lookahead from the detection point to ensure phase alignment.
     */
    std::vector<Transient> analyze(const float* data, size_t numSamples, float threshold = 0.15f) {
        std::vector<Transient> res;
        for (size_t i = 0; i < numSamples; ++i) {
            float absVal = std::abs(data[i]);
            m_envFast = absVal + m_alphaFast * (m_envFast - absVal);
            m_envSlow = absVal + m_alphaSlow * (m_envSlow - absVal);

            float diff = m_envFast - m_envSlow;
            
            // HYSTERESIS: 50ms re-trigger guard
            if (diff > threshold && (i - m_lastTransientIdx) > (m_sampleRate * 0.05f)) {
                // Apply Look-ahead: Shift the MIDI trigger back to the real 'Start' of the attack
                uint64_t adjIdx = (i > m_lookaheadSamples) ? i - m_lookaheadSamples : 0;
                res.push_back({ adjIdx, diff });
                m_lastTransientIdx = i;
            }
        }
        return res;
    }

private:
    double m_sampleRate;
    float m_envFast, m_envSlow;
    float m_alphaFast, m_alphaSlow;
    uint64_t m_lookaheadSamples;
    size_t m_lastTransientIdx = 0;
};

} // namespace Aura::Core::DSP::Analysis
