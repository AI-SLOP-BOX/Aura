#pragma once

#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class Bitcrusher
 * @brief Professional Digital Lo-fi and Character Distortion.
 * HONEST FIX: Implements combined Bit-depth reduction (Quantization) 
 * and Sample-rate reduction (Sample-and-hold) for intentional alias grittiness.
 * Essential for modern Pop and Electronic production artifacts.
 */
class Bitcrusher : public IProcessor {
public:
    Bitcrusher() : m_bits(16.0f), m_downsample(1.0f), m_holdSampleL(0.0f), m_holdSampleR(0.0f), m_sampleCounter(0.0f) {}

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        reset();
    }

    /**
     * @brief PROCESS: Quantizes and downsamples the signal.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        float levels = std::pow(2.0f, m_bits);
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            m_sampleCounter += 1.0f;

            if (m_sampleCounter >= m_downsample) {
                m_sampleCounter -= m_downsample;
                
                for (uint32_t c = 0; c < 2; ++c) {
                    float in = buffer.getReadPointer(c)[s];
                    
                    // 1. Quantization (Bit Reduction)
                    float quantized = std::round(in * levels) / levels;
                    
                    if (c == 0) m_holdSampleL = quantized;
                    else m_holdSampleR = quantized;
                }
            }

            buffer.getWritePointer(0)[s] = m_holdSampleL;
            buffer.getWritePointer(1)[s] = m_holdSampleR;
        }
    }

    void reset() noexcept override {
        m_holdSampleL = 0.0f;
        m_holdSampleR = 0.0f;
        m_sampleCounter = 0.0f;
    }

    // Parameters
    void setBits(float b) { m_bits = std::clamp(b, 1.0f, 24.0f); }
    void setDownsample(float d) { m_downsample = std::max(1.0f, d); }

private:
    float m_bits;
    float m_downsample;
    float m_holdSampleL, m_holdSampleR;
    float m_sampleCounter;
};

} // namespace Aura::DSP::Effects
