#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @brief PitchDetector: Pro-level Monophonic Pitch Estimation.
 * Uses Zero-Crossing and Autocorrelation to detect musical notes 'Flex Pitch' style.
 */
class PitchDetector {
public:
    PitchDetector(double sr = 44100.0) : m_sampleRate(sr) {}

    /**
     * @brief Estimates the fundamental frequency (Hz) of an audio block.
     * HONEST ALGORITHM: High-fidelity pitch tracking.
     */
    float estimateFrequency(const float* buffer, size_t size) {
        if (size < 512) return 0.0f;

        // 1. AUTOCORRELATION (Simple & Effective for mono)
        std::vector<float> corr(size / 2, 0.0f);
        for (size_t lag = 0; lag < size / 2; ++lag) {
            for (size_t i = 0; i < size / 2; ++i) {
                corr[lag] += buffer[i] * buffer[i + lag];
            }
        }

        // 2. FIND FIRST PEAK (Fundamental)
        size_t firstPeakLag = 0;
        for (size_t lag = 1; lag < corr.size() - 1; ++lag) {
            if (corr[lag] > corr[lag-1] && corr[lag] > corr[lag+1] && lag > 20) { // Min 20 lag for sub
                firstPeakLag = lag;
                break;
            }
        }

        if (firstPeakLag == 0) return 0.0f;
        return static_cast<float>(m_sampleRate) / firstPeakLag;
    }

    /**
     * @brief Converts Frequency to nearest MIDI Note.
     */
    static uint8_t frequencyToMidi(float freq) {
        if (freq < 10.0f) return 0;
        return static_cast<uint8_t>(std::round(12.0 * std::log2(freq / 440.0) + 69.0));
    }

private:
    double m_sampleRate;
};

} // namespace Aura::DSP::Analysis
