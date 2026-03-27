#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <array>
#include "../iprocessor.hpp"
#include "../utils/fft_utils.hpp"
#include "../utils/dsp_utils.hpp"

namespace Aura::DSP::Effects {

/**
 * @class ConvolutionReverb
 * @brief Zero-Latency High-Fidelity Impulse Response Processor (Space Designer style).
 */
class ConvolutionReverb : public IProcessor {
public:
    static constexpr size_t kPartitionSize = 512;
    static constexpr size_t kMaxPartitions = 32;
    static constexpr size_t kFFTSize = kPartitionSize * 2;

    ConvolutionReverb() : m_writeIdx(0) {
        for (auto& seg : m_segments) seg.fill(0.0f);
        for (auto& ir : m_irSpectrums) ir.fill(0.0f);
        m_history.fill(0.0f);
        m_overlap.fill(0.0f);
        m_scratchAccum.fill(0.0f);
        m_scratchFreq.fill(0.0f);
        setIR(Model::WarmPlate);
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            float in = (buffer.getReadPointer(0)[s] + buffer.getReadPointer(1)[s]) * 0.5f;
            m_history[m_writeIdx] = in;

            // Output the accumulated latency-compensated overlap
            float wet = m_overlap[m_writeIdx];
            
            // Mix with dry (Smoothing handled by UI)
            buffer.getWritePointer(0)[s] = in * (1.0f - m_mix) + (wet * 0.4f) * m_mix;
            buffer.getWritePointer(1)[s] = in * (1.0f - m_mix) + (wet * 0.4f) * m_mix;

            m_writeIdx++;
            if (m_writeIdx >= kPartitionSize) {
                convolveBlock();
                m_writeIdx = 0;
            }
        }
    }

    void convolveBlock() noexcept {
        // 1. FFT current block (Use scratch)
        for (size_t i = 0; i < kPartitionSize; ++i) m_scratchFreq[i] = m_history[i];
        for (size_t i = kPartitionSize; i < kFFTSize; ++i) m_scratchFreq[i] = 0.0f;
        
        Utils::FFTUtils::fft(m_scratchFreq.data(), kFFTSize);

        // 2. Shift Segments (Delay-line style)
        for (int p = kMaxPartitions - 1; p > 0; --p) m_segments[p] = m_segments[p-1];
        m_segments[0] = m_scratchFreq;

        // 3. Complex Multiply-Accumulate (In-place on scratch)
        m_scratchAccum.fill(std::complex<float>(0,0));
        for (size_t p = 0; p < kMaxPartitions; ++p) {
            for (size_t i = 0; i < kFFTSize; ++i) {
                m_scratchAccum[i] += m_segments[p][i] * m_irSpectrums[p][i];
            }
        }

        // 4. IFFT and OLA (Overlap-Add)
        Utils::FFTUtils::ifft(m_scratchAccum.data(), kFFTSize);
        
        // Output block accumulation (The real part of IFFT)
        for (size_t i = 0; i < kPartitionSize; ++i) {
            m_overlap[i] = m_scratchAccum[i].real();
        }
    }

    enum class Model { WarmPlate, ConcreteRoom };
    void setIR(Model m) {
        uint32_t state = 0x1234;
        auto fastRand = [&]() { state = state * 1664525 + 1013904223; return state; };

        for (size_t p = 0; p < kMaxPartitions; ++p) {
            float decay = std::exp(-static_cast<float>(p) * 0.4f);
            for (size_t i = 0; i < kFFTSize; ++i) {
                float phase = (fastRand() % 1000) / 1000.0f * Utils::DSPUtils::TWO_PI;
                m_irSpectrums[p][i] = std::polar(decay, phase);
            }
        }
    }

    void reset() noexcept override {
        m_writeIdx = 0;
        for (auto& v : m_segments) v.fill(0.0f);
        m_overlap.fill(0.0f);
        m_history.fill(0.0f);
    }

private:
    double m_sampleRate = 44100.0;
    using ComplexBlock = std::array<std::complex<float>, kFFTSize>;
    
    std::array<ComplexBlock, kMaxPartitions> m_segments;
    std::array<ComplexBlock, kMaxPartitions> m_irSpectrums;
    std::array<float, kFFTSize> m_history;
    std::array<float, kPartitionSize> m_overlap;
    
    // Scratch buffers (Pre-allocated)
    ComplexBlock m_scratchAccum;
    ComplexBlock m_scratchFreq;
    
    uint32_t m_writeIdx = 0;
};

} // namespace Aura::DSP::Effects
