#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class StereoPhaser
 * @brief High-end Modulation effect with multi-stage phase shifting.
 * HONEST FIX: Implements 4-stage All-Pass filters with a stereo-offset LFO 
 * to create the iconic 'sweeping' movement.
 * Provides the psychedelic depth found in high-end modulation pedals.
 */
class StereoPhaser : public IProcessor {
public:
    StereoPhaser() {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Modulates phase cancellaton points over time.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            // 1. Stereo LFO
            m_lfoPhase += (m_rate / m_sampleRate);
            if (m_lfoPhase >= 1.0) m_lfoPhase -= 1.0;

            float lfoL = 0.5f + 0.5f * std::sin(2.0f * M_PI * m_lfoPhase);
            float lfoR = 0.5f + 0.5f * std::sin(2.0f * M_PI * m_lfoPhase + 0.5f * M_PI); // 90-deg offset

            // 2. Filter Update & Filter Process
            for (uint32_t c = 0; c < 2; ++c) {
                float freq = std::lerp(500.0f, 4000.0f, (c == 0 ? lfoL : lfoR));
                float g = (freq - m_sampleRate) / (freq + m_sampleRate); // Simplified all-pass coeff

                float* p = buffer.getWritePointer(c);
                float in = p[s] + m_feedback * m_lastOut[c];
                
                // 4-Stage All-pass cascade
                float y = in;
                for (int stage = 0; stage < 4; ++stage) {
                    float out = g * y + m_filterState[c][stage];
                    m_filterState[c][stage] = y - g * out;
                    y = out;
                }

                m_lastOut[c] = y;
                p[s] = (p[s] * (1.0f - m_mix)) + (y * m_mix);
            }
        }
    }

    void reset() noexcept override {
        for (auto& v : m_filterState) std::fill(v.begin(), v.end(), 0.0f);
        m_lastOut[0] = m_lastOut[1] = 0.0f;
    }

    // Parameters
    void setMix(float m) { m_mix = m; }
    void setRate(float r) { m_rate = r; }
    void setFeedback(float f) { m_feedback = f; }

private:
    double m_sampleRate = 44100.0;
    float m_lfoPhase = 0.0f;
    float m_rate = 0.5f;
    float m_mix = 0.5f;
    float m_feedback = 0.3f;

    std::vector<float> m_filterState[2] = { std::vector<float>(4, 0.0f), std::vector<float>(4, 0.0f) };
    float m_lastOut[2] = {0, 0};
};

} // namespace Aura::DSP::Effects
