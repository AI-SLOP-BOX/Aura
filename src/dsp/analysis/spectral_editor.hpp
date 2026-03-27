#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include "fft_engine.hpp"

namespace Aura::DSP::Analysis {

/**
 * @brief SpectralEditor: Professional surgical frequency repair.
 * Now powered by a real FFT Engine for precision cleaning.
 */
class SpectralEditor {
public:
    SpectralEditor(uint32_t fftSize = 1024) : m_fftSize(fftSize), m_fftEngine(fftSize) {
        m_complexBuffer.resize(fftSize);
        m_magnitudeBuffer.resize(fftSize / 2 + 1, 0.0f);
    }

    /**
     * @brief ANALYZE: Performs STFT (Forward FFT) to identify energy.
     */
    void analyze(const std::vector<float>& timeDomainIn) {
        m_fftEngine.forward(timeDomainIn, m_complexBuffer);
        for (uint32_t i = 0; i < m_magnitudeBuffer.size(); ++i) {
            m_magnitudeBuffer[i] = std::abs(m_complexBuffer[i]);
        }
    }

    /**
     * @brief APPLY MASK: Surgically zero out frequency bins.
     */
    void applyMask(uint32_t fMinBin, uint32_t fMaxBin, float attenuation = 0.0f) {
        for (uint32_t i = fMinBin; i <= fMaxBin && i < m_complexBuffer.size(); ++i) {
            m_complexBuffer[i] *= attenuation;
            // Also zero out conjugate reflection if needed for real-IFFT
            if (i > 0 && i < m_fftSize) m_complexBuffer[m_fftSize - i] = std::conj(m_complexBuffer[i]);
        }
    }

    /**
     * @brief RECONSTRUCT: Performs Inverse-FFT to get cleaned audio.
     */
    std::vector<float> reconstruct() {
        std::vector<float> out(m_fftSize);
        m_fftEngine.inverse(m_complexBuffer, out);
        return out;
    }

private:
    uint32_t m_fftSize;
    FFTEngine m_fftEngine;
    std::vector<std::complex<float>> m_complexBuffer;
    std::vector<float> m_magnitudeBuffer;
};

} // namespace Aura::DSP::Analysis
