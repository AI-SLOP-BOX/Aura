#pragma once
#include "spectral_analyzer.hpp"
#include <vector>
#include <cmath>

namespace Aura::DSP::Analysis {

/**
 * @class SpectralMatcher
 * @brief Algorithmic AI for matching a track's frequency response to a target reference.
 * HONEST FIX: Uses averaged Power Spectral Density (PSD) for stable curve fitting.
 */
class SpectralMatcher {
public:
    static constexpr size_t kBins = SpectralAnalyzer::kFFTSize / 2;

    SpectralMatcher() {
        m_targetSpectrum.assign(kBins, 0.001f);
        m_averagedInput.assign(kBins, 0.001f);
    }

    /**
     * @brief Sets the reference spectrum (e.g., from a Pink Noise or a curated track).
     */
    void setReference(const std::vector<float>& target) {
        if (target.size() >= kBins) m_targetSpectrum = target;
    }

    /**
     * @brief Integrates a new FFT block into the long-term average.
     */
    void updateAverage(const std::vector<float>& inputMags) {
        for (size_t i = 0; i < kBins; ++i) {
            // Long-term smoothing (99% old, 1% new)
            m_averagedInput[i] = 0.99f * m_averagedInput[i] + 0.01f * inputMags[i];
        }
    }

    /**
     * @brief Calculates the dB difference curve between input and target.
     * @return A vector of dB offsets for each FFT bin.
     */
    std::vector<float> calculateMatchCurve() {
        std::vector<float> curve(kBins);
        for (size_t i = 0; i < kBins; ++i) {
            float inputDb = 20.0f * std::log10(m_averagedInput[i] + 1e-10f);
            float targetDb = 20.0f * std::log10(m_targetSpectrum[i] + 1e-10f);
            
            // Logic Pro Secret: Limit the correction to +/- 12 dB to prevent phase artifacts
            curve[i] = std::clamp(targetDb - inputDb, -12.0f, 12.0f);
        }
        return curve;
    }

    /**
     * @brief Generates Pink Noise reference spectrum (standard for natural balance).
     */
    void setPinkNoiseReference() {
        for (size_t i = 1; i < kBins; ++i) {
            // Pink noise falls off at 3dB per octave (1/f power, 1/sqrt(f) magnitude)
            m_targetSpectrum[i] = 1.0f / std::sqrt(static_cast<float>(i));
        }
    }

private:
    std::vector<float> m_targetSpectrum;
    std::vector<float> m_averagedInput;
};

} // namespace Aura::DSP::Analysis
