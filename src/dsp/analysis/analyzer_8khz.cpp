#include "analyzer_8khz.hpp"
#include <cmath>
#include <numeric>

namespace Aura::DSP::Analysis {

Analyzer8kHz::Analyzer8kHz(double sr) {
    setSampleRate(sr);
}

void Analyzer8kHz::setSampleRate(double sr) {
    m_sampleRate = sr;
    double f0 = 8000.0, Q = 0.707;
    double omega = 2.0 * 3.1415926535 * f0 / sr;
    double alpha = std::sin(omega) / (2.0 * Q);
    double a0 = 1.0 + alpha;
    m_b0 = alpha / a0; m_b1 = 0; m_b2 = -alpha / a0;
    m_a1 = -2.0 * std::cos(omega) / a0;
    m_a2 = (1.0 - alpha) / a0;
}

/**
 * @brief Analyzer8kHz: Specialized spectral analyzer for psychoacoustic optimization.
 */
void Analyzer8kHz::analyze(const float* buffer, size_t numFrames) {
    if (numFrames == 0) return;

    float sumSq = 0.0f;
    for (size_t i = 0; i < numFrames; ++i) {
        float x = buffer[i];
        float y = m_b0 * x + m_b1 * m_z1 + m_b2 * m_z2 - m_a1 * m_z1 - m_a2 * m_z2;
        
        m_z2 = m_z1;
        m_z1 = y;

        sumSq += y * y;
    }
    
    float rms = std::sqrt(sumSq / std::max((size_t)1, numFrames));
    m_highFreqEnergy.store(rms, std::memory_order_relaxed);
}


float Analyzer8kHz::getEnergy() const {
    return m_highFreqEnergy.load();
}

} // namespace Aura::DSP::Analysis
