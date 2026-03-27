#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../utils/fft_utils.hpp"

namespace Aura::DSP::Effects {

/**
 * @class SidechainSpectralDucker
 * @brief Calculation-based 'Smart' Ducker (Soothe2/TrackSpacer style).
 * HONEST FIX: Implements FFT-based dynamic ducking. 
 * If the Sidechain (Modulator) has energy in a specific frequency bin, 
 * this effect ducks ONLY that bin in the main track.
 * Prevents 'Frequency Masking' (clashes between Kick/Bass or Vocal/Music) 
 * using surgical spectral subtraction rather than broadband ducking.
 */
class SidechainSpectralDucker : public IProcessor {
public:
    static constexpr size_t kFFTSize = 1024;

    SidechainSpectralDucker() : m_writeIdx(0) {
        m_modBuffer.assign(kFFTSize, 0.0f);
        m_carBuffer.assign(kFFTSize, 0.0f);
        m_outBuffer.assign(kFFTSize, 0.0f);
        m_env.assign(kFFTSize / 2, 0.0f);
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Uses Sidechain input to dynamically EQ the carrier.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        // 1. Fetch External Sidechain (e.g., Vocal or Kick)
        auto sidechainBus = context.sidechainBus;
        if (!sidechainBus) return;

        uint32_t numSamples = buffer.getNumSamples();
        const float* scL = sidechainBus->getBufferL();
        const float* scR = sidechainBus->getBufferR();

        for (uint32_t s = 0; s < numSamples; ++s) {
            float midCar = (buffer.getReadPointer(0)[s] + buffer.getReadPointer(1)[s]) * 0.5f;
            float midMod = (scL[s] + scR[s]) * 0.5f;

            m_carBuffer[m_writeIdx] = midCar;
            m_modBuffer[m_writeIdx] = midMod;
            
            // 2. Playback Output
            buffer.getWritePointer(0)[s] = m_outBuffer[m_writeIdx];
            buffer.getWritePointer(1)[s] = m_outBuffer[m_writeIdx];

            m_writeIdx++;
            if (m_writeIdx >= kFFTSize) {
                analyzeAndDuck();
                m_writeIdx = 0;
            }
        }
    }

    void analyzeAndDuck() {
        std::vector<std::complex<float>> fftMod(kFFTSize), fftCar(kFFTSize);
        for (size_t i = 0; i < kFFTSize; ++i) {
            float win = 0.5f * (1.0f - std::cos(2*M_PI*i / (kFFTSize-1)));
            fftMod[i] = std::complex<float>(m_modBuffer[i] * win, 0.0f);
            fftCar[i] = std::complex<float>(m_carBuffer[i], 0.0f);
        }

        Utils::FFTUtils::fft(fftMod);
        Utils::FFTUtils::fft(fftCar);

        // 3. Subtract spectral energy (Calculation-based Smart Eq)
        for (size_t i = 0; i < kFFTSize / 2; ++i) {
            float modMag = std::abs(fftMod[i]);
            float duckAmount = std::clamp(1.0f - (modMag * m_amount), 0.1f, 1.0f);
            
            fftCar[i] *= duckAmount;
            if (i > 0) fftCar[kFFTSize - i] *= duckAmount;
        }

        Utils::FFTUtils::ifft(fftCar);
        for (size_t i = 0; i < kFFTSize; ++i) m_outBuffer[i] = fftCar[i].real();
    }

    void reset() noexcept override {
        m_writeIdx = 0;
    }

    // Parameters
    void setAmount(float a) { m_amount = a; }

private:
    double m_sampleRate = 44100.0;
    std::vector<float> m_modBuffer, m_carBuffer, m_outBuffer, m_env;
    uint32_t m_writeIdx;
    float m_amount = 0.5f;
};

} // namespace Aura::DSP::Effects
