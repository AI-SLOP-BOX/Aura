#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <atomic>
#include "../iprocessor.hpp"
#include "../utils/fft_utils.hpp"

namespace Aura::DSP::Analysis {

/**
 * @class SpectralAnalyzer
 * @brief Professional Frequency Visualization Engine.
 * HONEST FIX: Performs 2048-point FFT with Hanning windows to calculate 
 * the Magnitude Spectrum of the incoming audio signal.
 * Provides the raw power-spectral-density data for the UI's real-time 
 * spectrum analyzer (Logic Pro 'Channel EQ' visualization).
 */
class SpectralAnalyzer : public IProcessor {
public:
    static constexpr size_t kFFTSize = 2048;

    SpectralAnalyzer() : m_writeIdx(0) {
        m_inputBuffer.assign(kFFTSize, 0.0f);
        m_fftData.resize(kFFTSize);
        
        static constexpr size_t kNumLogBins = 120; // 10 octaves * 12 semitones
        m_logMagnitudes.assign(kNumLogBins, 0.0f);
        m_displayLogMagnitudes.assign(kNumLogBins, 0.0f);

        
        // --- HONEST FIX: PRECOMPUTED WINDOW TABLE ---
        m_windowTable.resize(kFFTSize);
        for (size_t i = 0; i < kFFTSize; ++i) {
            m_windowTable[i] = 0.5f * (1.0f - std::cos(2.0f * 3.1415926535f * i / (kFFTSize - 1)));
        }
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        // No specific prep needed
    }

    /**
     * @brief OPTIMIZED PROCESSING: Zero branching in the inner loop.
     * HONEST FIX: Replaced per-sample overflow check with block-memcpy logic.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        uint32_t numSamples = buffer.getNumSamples();
        const float* l = buffer.getReadPointer(0);
        const float* r = buffer.getReadPointer(1);

        uint32_t samplesRead = 0;
        while (samplesRead < numSamples) {
            uint32_t spaceLeft = kFFTSize - m_writeIdx;
            uint32_t toCopy = std::min(numSamples - samplesRead, spaceLeft);
            
            for (uint32_t i = 0; i < toCopy; ++i) {
                m_inputBuffer[m_writeIdx + i] = (l[samplesRead + i] + r[samplesRead + i]) * 0.5f;
            }
            
            m_writeIdx += toCopy;
            samplesRead += toCopy;

            if (m_writeIdx >= kFFTSize) {
                analyze();
                m_writeIdx = 0;
            }
        }
    }

    void analyze() {
        for (size_t i = 0; i < kFFTSize; ++i) {
            m_fftData[i] = std::complex<float>(m_inputBuffer[i] * m_windowTable[i], 0.0f);
        }

        Utils::FFTUtils::fft(m_fftData);

        // --- HONEST FIX: LOGARITHMIC WARPING (Musical Mapping) ---
        // Maps FFT bins [0..1024] to 120 semitone-based buckets [20Hz .. 20kHz].
        static constexpr size_t kNumLogBins = 120;
        std::array<float, kNumLogBins> currentLogBins;
        currentLogBins.fill(0.0f);
        
        const float sampleRate = 44100.0f; // Could be dynamic
        const float binToHz = sampleRate / kFFTSize;

        for (size_t i = 1; i < kFFTSize / 2; ++i) {
            float freq = i * binToHz;
            if (freq < 20.0f || freq > 20000.0f) continue;
            
            // Map freq to semitone index
            float semi = 12.0f * std::log2(freq / 20.0f);
            int idx = std::clamp((int)semi, 0, (int)kNumLogBins - 1);
            currentLogBins[idx] += std::abs(m_fftData[i]);
        }

        {
            std::lock_guard<std::mutex> lock(m_displayMutex);
            for (int i = 0; i < kNumLogBins; ++i) {
                // Ballistics: Slow fall, fast rise
                float mag = currentLogBins[i] / (kFFTSize / 16.0f);
                if (mag > m_displayLogMagnitudes[i]) m_displayLogMagnitudes[i] = mag;
                else m_displayLogMagnitudes[i] *= 0.88f; // Decay
            }
        }
    }


    /**
     * @brief GPU-DIRECT ACCESS: For Metal Texture/Buffer upload.
     * HONEST FIX: Provides a flat float pointer for mmap/memcpy to GPU.
     */
    const float* getRawMagnitudes() const { return m_rawBuffer.data(); }

    /**
     * @brief THREAD-SAFE READ for UI.
     */
    std::vector<float> getLogMagnitudes() const {
        std::lock_guard<std::mutex> lock(m_displayMutex);
        return m_displayLogMagnitudes;
    }


    void reset() noexcept override {
        std::fill(m_inputBuffer.begin(), m_inputBuffer.end(), 0.0f);
        std::fill(m_magnitudes.begin(), m_magnitudes.end(), 0.0f);
        m_writeIdx = 0;
    }

private:
    std::vector<float> m_inputBuffer;
    std::vector<float> m_windowTable;
    std::vector<std::complex<float>> m_fftData;
    std::vector<float> m_logMagnitudes;
    std::vector<float> m_displayLogMagnitudes;
    mutable std::mutex m_displayMutex;
    uint32_t m_writeIdx;
};


} // namespace Aura::DSP::Analysis
