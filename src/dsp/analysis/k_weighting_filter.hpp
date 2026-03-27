#pragma once
#include <cmath>

namespace Aura::DSP::Analysis {

/**
 * @brief KWeightingFilter: Standardized frequency weighting for LUFS measurement.
 * Reference: ITU-R BS.1770-4.
 */
class KWeightingFilter {
public:
    KWeightingFilter(double sr = 44100.0) : m_sampleRate(sr) {
        setupCoefficients();
    }

    void process(float l, float r, float& outL, float& outR) {
        // Stage 1: High Shelf (Pre-filter)
        float vL1 = m_b0_stage1 * l + m_b1_stage1 * m_z1L_stage1 + m_b2_stage1 * m_z2L_stage1 - m_a1_stage1 * m_z1L_stage1 - m_a2_stage1 * m_z2L_stage1;
        m_z2L_stage1 = m_z1L_stage1; m_z1L_stage1 = vL1;
        float vR1 = m_b0_stage1 * r + m_b1_stage1 * m_z1R_stage1 + m_b2_stage1 * m_z2R_stage1 - m_a1_stage1 * m_z1R_stage1 - m_a2_stage1 * m_z2R_stage1;
        m_z2R_stage1 = m_z1R_stage1; m_z1R_stage1 = vR1;

        // Stage 2: High Pass (RLB-filter)
        outL = m_b0_stage2 * vL1 + m_b1_stage2 * m_z1L_stage2 + m_b2_stage2 * m_z2L_stage2 - m_a1_stage2 * m_z1L_stage2 - m_a2_stage2 * m_z2L_stage2;
        m_z2L_stage2 = m_z1L_stage2; m_z1L_stage2 = outL;
        outR = m_b0_stage2 * vR1 + m_b1_stage2 * m_z1R_stage2 + m_b2_stage2 * m_z2R_stage2 - m_a1_stage2 * m_z1R_stage2 - m_a2_stage2 * m_z2R_stage2;
        m_z2R_stage2 = m_z1R_stage2; m_z1R_stage2 = outR;
    }

private:
    void setupCoefficients() {
        // ITU-R BS.1770-4 Standard Coefficients (Pre-filter: High-shelf)
        double fs = m_sampleRate;
        double vh = 3.999843853973347;
        double qh = 0.7071752369554193;
        double fh = 1681.974450955531;

        double K  = tan(M_PI * fh / fs);
        double common = 1.0 + K / qh + K * K;
        m_b0_stage1 = static_cast<float>((vh + sqrt(vh) * K / qh + K * K) / common);
        m_b1_stage1 = static_cast<float>(2.0 * (K * K - vh) / common);
        m_b2_stage1 = static_cast<float>((vh - sqrt(vh) * K / qh + K * K) / common);
        m_a1_stage1 = static_cast<float>(2.0 * (K * K - 1.0) / common);
        m_a2_stage1 = static_cast<float>((1.0 - K / qh + K * K) / common);

        // Stage 2: RLB (High-pass)
        double f0 = 38.13547087613982;
        double q0 = 0.5003270373253953;
        K = tan(M_PI * f0 / fs);
        common = 1.0 + K / q0 + K * K;
        m_b0_stage2 = 1.0f;
        m_b1_stage2 = -2.0f;
        m_b2_stage2 = 1.0f;
        m_a1_stage2 = static_cast<float>(2.0 * (K * K - 1.0) / common);
        m_a2_stage2 = static_cast<float>((1.0 - K / q0 + K * K) / common);
    }

    double m_sampleRate;
    float m_z1L_stage1 = 0, m_z2L_stage1 = 0, m_z1R_stage1 = 0, m_z2R_stage1 = 0;
    float m_z1L_stage2 = 0, m_z2L_stage2 = 0, m_z1R_stage2 = 0, m_z2R_stage2 = 0;
    float m_b0_stage1, m_b1_stage1, m_b2_stage1, m_a1_stage1, m_a2_stage1;
    float m_b0_stage2, m_b1_stage2, m_b2_stage2, m_a1_stage2, m_a2_stage2;
};

} // namespace Aura::DSP::Analysis
