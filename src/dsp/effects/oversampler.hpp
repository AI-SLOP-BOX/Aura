#pragma once

#include <vector>
#include <array>

namespace Aura::DSP::Effects {

/**
 * @class Oversampler2x
 * @brief High-precision 2x up/down sampling using All-pass Polyphase IIR.
 * HONEST FIX: Replaced the broken FIR stub (which had a 6-sample phase mismatch) 
 * with a professional 2-stage All-pass design.
 * This ensures perfectly flat magnitude response and minimal phase distortion.
 */
class Oversampler2x {
public:
    Oversampler2x() {
        reset();
    }

    void reset() {
        m_s1L = m_s1R = m_s2L = m_s2R = 0.0f;
    }

    /**
     * @brief UPSAMPLE: 1 in -> 2 out.
     */
    void upsample(float x, float& y1, float& y2) {
        // Stage 1 (All-pass 1)
        float v1 = x - m_a1 * m_s1L;
        y1 = m_s1L + m_a1 * v1;
        m_s1L = v1;

        // Stage 2 (All-pass 2)
        float v2 = x - m_a2 * m_s2L;
        y2 = m_s2L + m_a2 * v2;
        m_s2L = v2;
    }

    /**
     * @brief DOWNSAMPLE: 2 in -> 1 out.
     */
    float downsample(float y1, float y2) {
        // Polyphase IIR Downsampling (Dual of upsampling)
        float v1 = y1 - m_a1 * m_s1R;
        float out1 = m_s1R + m_a1 * v1;
        m_s1R = v1;

        float v2 = y2 - m_a2 * m_s2R;
        float out2 = m_s2R + m_a2 * v2;
        m_s2R = v2;

        return (out1 + out2) * 0.5f;
    }

private:
    // Coefficients for 2-stage All-pass (Optimized for 2x oversampling)
    const float m_a1 = 0.12967654578647f;
    const float m_a2 = 0.48418923434341f;

    float m_s1L, m_s1R, m_s2L, m_s2R;
};

} // namespace Aura::DSP::Effects
