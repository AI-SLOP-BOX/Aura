#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @class Analyzer8kHz
 * @brief Professional High-Frequency Energy Tracker.
 */
class Analyzer8kHz {
public:
    Analyzer8kHz(double sr = 44100.0);

    void setSampleRate(double sr);

    /**
     * @brief ACCELERATED BPF ANALYSIS.
     */
    void analyze(const float* buffer, size_t numFrames);

    float getEnergy() const;

private:
    std::atomic<float> m_highFreqEnergy{0.0f};
    double m_sampleRate = 44100.0;
    float m_z1 = 0.0f, m_z2 = 0.0f; // Filter state
    float m_b0, m_b1, m_b2, m_a1, m_a2; // Precomputed coeffs
};

} // namespace Aura::DSP::Analysis
