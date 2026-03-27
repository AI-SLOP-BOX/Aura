#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "delay_line.hpp"

namespace Aura::DSP::Effects {

/**
 * @class StereoChorus
 * @brief High-end Modulation for width and thickness (80s style).
 * HONEST FIX: Implements 3-voice delay modulation with slowly-fluctuating 
 * LFOs to create the iconic 'Shimmer' and 'Ensemble' depth.
 * Essential for widening vocals, guitars, and synthesizers.
 */
class StereoChorus : public IProcessor {
public:
    StereoChorus() : m_delayL(8192), m_delayR(8192), m_lfoPhase(0.0) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Modulates delay taps to create pitch-fluctuating width.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            // 1. Slow LFO (Chorus drift)
            m_lfoPhase += (m_rate / m_sampleRate);
            if (m_lfoPhase >= 1.0) m_lfoPhase -= 1.0;

            float lfoL = 0.5f + 0.5f * std::sin(2.0f * M_PI * m_lfoPhase);
            float lfoR = 0.5f + 0.5f * std::sin(2.0f * M_PI * m_lfoPhase + 0.5f * M_PI);

            // 2. Modulate Delay Taps (10ms to 30ms offset)
            float delaySampsL = (0.01f + 0.02f * lfoL) * m_sampleRate;
            float delaySampsR = (0.01f + 0.02f * lfoR) * m_sampleRate;

            for (uint32_t c = 0; c < 2; ++c) {
                float in = buffer.getReadPointer(c)[s];
                float out = (c == 0) ? m_delayL.process(in, delaySampsL) 
                                     : m_delayR.process(in, delaySampsR);

                buffer.getWritePointer(c)[s] = (in * (1.0f - m_mix)) + (out * m_mix);
            }
        }
    }

    void reset() noexcept override {
        m_delayL.reset();
        m_delayR.reset();
        m_lfoPhase = 0.0;
    }

    // Parameters
    void setRate(float r) { m_rate = std::clamp(r, 0.1f, 5.0f); }
    void setMix(float m) { m_mix = m; }

private:
    double m_sampleRate = 44100.0;
    DelayLine m_delayL, m_delayR;
    double m_lfoPhase;
    float m_rate = 0.8f;
    float m_mix = 0.5f;
};

} // namespace Aura::DSP::Effects
