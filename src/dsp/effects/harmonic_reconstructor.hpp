#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/audio_buffer.hpp"
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class HarmonicReconstructor
 * @brief High-frequency 'Air' restoration using non-linear projection.
 */
class HarmonicReconstructor : public IProcessor {
public:
    HarmonicReconstructor(double sr = 44100.0) : m_sampleRate(sr) {}

    void prepareToPlay(double sr, uint32_t bs) noexcept override { m_sampleRate = sr; }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;
        
        float cutoff = 12000.0f; 
        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
            float* p = buffer.getWritePointer(c);
            for (uint32_t s = 0; s < numSamples; ++s) {
                float in = p[s];
                float high = std::tanh(in * 1.5f) - in;
                p[s] += high * 0.1f;
            }
        }
    }

private:
    void updateCoefficients() {
        float dt = 1.0f / m_sampleRate;
        float rc = 1.0f / (2.0f * M_PI * m_cutoff);
        m_alpha = rc / (rc + dt);
        m_hpfState.assign(32, 0.0f); // Support up to 32 channels
        m_lastIn.assign(32, 0.0f);
    }

    float m_cutoff;
    float m_sampleRate;
    float m_alpha = 0.9f;
    std::vector<float> m_hpfState;
    std::vector<float> m_lastIn;
};

} // namespace Aura::DSP::Effects
