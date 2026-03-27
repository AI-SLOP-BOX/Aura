#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/audio_buffer.hpp"
#include "../../core/concurrency/simd_kernel.hpp"
#include "../iprocessor.hpp"
#include "state_variable_filter.hpp"

namespace Aura::DSP::Effects {

/**
 * @class VirtuosoPultec
 * @brief Legendary Passive Program Equalizer (EQP-1A Emulation).
 */
class VirtuosoPultec : public IProcessor {
public:
    VirtuosoPultec(double sr = 44100.0) : m_sampleRate(sr), m_lowShelf(sr), m_highShelf(sr) {
        m_lowShelf.setType(StateVariableFilter::Type::LowShelf);
        m_highShelf.setType(StateVariableFilter::Type::HighShelf);
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        m_lowShelf.prepareToPlay(sr, bs);
        m_highShelf.prepareToPlay(sr, bs);
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        // 1. LOW END (Boost + Atten)
        m_lowShelf.setParams(m_lowFreq, m_lowBoost - m_lowAtten, 0.707f);
        m_lowShelf.process(buffer);
        
        // 2. HIGH END (Smooth Air)
        m_highShelf.setParams(m_highFreq, m_highBoost, 0.5f);
        m_highShelf.process(buffer);
        
        // 3. TUBE WARMTH
        uint32_t numSamples = buffer.getNumSamples();
        for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
            float* samples = buffer.getWritePointer(c);
            for (uint32_t s = 0; s < numSamples; ++s) {
                samples[s] = std::tanh(samples[s] * 1.05f) * 0.95f;
            }
        }
    }

    void reset() noexcept override {
        m_lowShelf.reset();
        m_highShelf.reset();
    }

private:
    double m_sampleRate;
    float m_lowFreq = 60.0f, m_lowBoost = 2.0f, m_lowAtten = 1.0f;
    float m_highFreq = 12000.0f, m_highBoost = 3.0f;
    
    StateVariableFilter m_lowShelf, m_highShelf;
};

} // namespace Aura::DSP::Effects
