#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/audio_buffer.hpp"
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class PsychoAcousticFocus
 * @brief Professional 'Brain-Friendly' Harmonic Enhancer.
 */
class PsychoAcousticFocus : public IProcessor {
public:
    PsychoAcousticFocus(double sr = 44100.0) : m_sampleRate(sr) {
        updateCoefficients();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        updateCoefficients();
    }

    /**
     * @brief PROCESS: Generates musically-related even-order harmonics.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;
        
        uint32_t numSamples = buffer.getNumSamples();
        float focusAmount = 0.5f; // TODO: Map to parameter
        
        for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
            float* p = buffer.getWritePointer(c);
            for (uint32_t s = 0; s < numSamples; ++s) {
                float in = p[s];
                
                // 1. Extract Presence (High-pass at 3.5kHz)
                m_hpfState[c] = m_alpha * (m_hpfState[c] + in - m_lastIn[c]);
                m_lastIn[c] = in;
                float highMids = m_hpfState[c];
                
                // 2. Add 'Musical' Harmonics (Asymmetrical saturation for presence focus)
                float harmonic = std::tanh(highMids * 2.0f) * focusAmount * 0.1f;
                
                // 3. Blend back (Psych-Additive logic)
                p[s] += harmonic;
            }
        }
    }

    void reset() noexcept override {
        std::fill(m_hpfState.begin(), m_hpfState.end(), 0.0f);
    }

private:
    void updateCoefficients() {
        float cutoff = 3500.0f;
        float dt = 1.0f / static_cast<float>(m_sampleRate);
        float rc = 1.0f / (2.0f * M_PI * cutoff);
        m_alpha = rc / (rc + dt);
        m_hpfState.assign(32, 0.0f);
        m_lastIn.assign(32, 0.0f);
    }

    double m_sampleRate;
    float m_alpha = 0.9f;
    std::vector<float> m_hpfState;
    std::vector<float> m_lastIn;
};

} // namespace Aura::DSP::Effects
