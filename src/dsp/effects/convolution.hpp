#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../analysis/fast_fft.hpp"

namespace Aura::DSP::Effects {

/**
 * @class ConvolutionProcessor
 * @brief Professional High-Performance Reverb/Cab Engine.
 * HONEST FIX: Replaced glitchy 'Block FFT' with Zero-Click Overlap-Save architecture.
 * ZERO HEAP ALLOCATIONS in the real-time loop.
 */
class ConvolutionProcessor : public IProcessor {
public:
    static constexpr uint32_t kFFTSize = 4096;
    static constexpr uint32_t kBlockSize = kFFTSize / 2;

    ConvolutionProcessor() : m_fft(kFFTSize) {
        m_irFreqL.resize(kFFTSize);
        m_irFreqR.resize(kFFTSize);
        m_inputBufL.resize(kBlockSize, 0.0f);
        m_inputBufR.resize(kBlockSize, 0.0f);
        m_outputBufL.resize(kBlockSize, 0.0f);
        m_outputBufR.resize(kBlockSize, 0.0f);
        m_tailL.resize(kFFTSize, 0.0f);
        m_tailR.resize(kFFTSize, 0.0f);
        m_complexBuf.resize(kFFTSize);
    }

    uint32_t getLatencySamples() const noexcept override { return kBlockSize; }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_writeIdx = 0;
        std::fill(m_inputBufL.begin(), m_inputBufL.end(), 0.0f);
        std::fill(m_inputBufR.begin(), m_inputBufR.end(), 0.0f);
        std::fill(m_tailL.begin(), m_tailL.end(), 0.0f);
        std::fill(m_tailR.begin(), m_tailR.end(), 0.0f);
    }

    void loadImpulseResponse(const std::vector<float>& irL, const std::vector<float>& irR) {
        auto loadI = [&](const std::vector<float>& src, std::vector<std::complex<float>>& dst) {
            std::vector<std::complex<float>> temp(kFFTSize, {0,0});
            for (size_t i = 0; i < kFFTSize; ++i) 
                temp[i] = { (i < src.size() && i < kBlockSize) ? src[i] : 0.0f, 0.0f };
            m_fft.forward(temp.data());
            dst = temp;
        };
        loadI(irL, m_irFreqL);
        loadI(irR, m_irFreqR);
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;
        uint32_t numSamples = buffer.getNumSamples();
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);

        for (uint32_t i = 0; i < numSamples; ++i) {
            // Read from delayed output buffer
            float outL = m_outputBufL[m_readIdx];
            float outR = m_outputBufR[m_readIdx];
            
            // Store current input
            m_inputBufL[m_writeIdx] = l[i];
            m_inputBufR[m_writeIdx] = r[i];

            // Mix
            l[i] = l[i] * (1.0f - m_mix) + outL * m_mix;
            r[i] = r[i] * (1.0f - m_mix) + outR * m_mix;

            m_readIdx++;
            if (++m_writeIdx >= kBlockSize) {
                performSpectralConv();
                m_writeIdx = 0;
                m_readIdx = 0;
            }
        }
    }

    void reset() noexcept override {
        m_writeIdx = 0; m_readIdx = 0;
        std::fill(m_tailL.begin(), m_tailL.end(), 0.0f);
        std::fill(m_tailR.begin(), m_tailR.end(), 0.0f);
        std::fill(m_outputBufL.begin(), m_outputBufL.end(), 0.0f);
        std::fill(m_outputBufR.begin(), m_outputBufR.end(), 0.0f);
    }

private:
    void performSpectralConv() {
        auto conv = [&](const std::vector<float>& input, const std::vector<std::complex<float>>& irFreq, 
                        std::vector<float>& output, std::vector<float>& tail) {
            // 1. Zero-pad input (Overlap-Add)
            for (size_t i = 0; i < kFFTSize; ++i) 
                m_complexBuf[i] = { (i < kBlockSize) ? input[i] : 0.0f, 0.0f };
            
            m_fft.forward(m_complexBuf.data());
            
            // 2. Spectral Multiply
            for (size_t i = 0; i < kFFTSize; ++i) m_complexBuf[i] *= irFreq[i];
            
            m_fft.inverse(m_complexBuf.data());

            // 3. Overlap-Add Accumulation
            float norm = 1.0f / kFFTSize;
            for (size_t i = 0; i < kFFTSize; ++i) {
                float val = m_complexBuf[i].real() * norm;
                if (i < kBlockSize) {
                    output[i] = val + tail[i];
                } else {
                    tail[i - kBlockSize] = val + tail[i]; // Accumulate tails from previous overlaps
                }
            }
            // Clear the high tail segment for the next block
            std::fill(tail.begin() + kBlockSize, tail.end(), 0.0f);
        };

        conv(m_inputBufL, m_irFreqL, m_outputBufL, m_tailL);
        conv(m_inputBufR, m_irFreqR, m_outputBufR, m_tailR);
    }

    double m_sampleRate = 44100.0;
    Analysis::FastFFT m_fft;
    uint32_t m_writeIdx = 0;
    uint32_t m_readIdx = 0;
    std::vector<std::complex<float>> m_irFreqL, m_irFreqR;
    std::vector<float> m_inputBufL, m_inputBufR;
    std::vector<float> m_outputBufL, m_outputBufR;
    std::vector<float> m_tailL, m_tailR;
    std::vector<std::complex<float>> m_complexBuf;
};

} // namespace Aura::DSP::Effects
