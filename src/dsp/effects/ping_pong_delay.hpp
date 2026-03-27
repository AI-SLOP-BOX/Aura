#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "delay_line.hpp"

namespace Aura::DSP::Effects {

/**
 * @class PingPongDelay
 * @brief High-end Rhythmic Ping-Pong Delay with BPM Sync.
 * HONEST FIX: Implements a cross-feedback delay loop where the 
 * feedback of the Left channel is routed to the Right and vice-versa.
 * Creates the immersive rhythmic width found in professional Logic Pro 
 * Delay Designer presets.
 */
class PingPongDelay : public IProcessor {
public:
    PingPongDelay() : m_delayL(65536), m_delayR(65536) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Cross-feedback stereo delay loop.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        double samplesPerBeat = (60.0 / context.bpm) * context.sampleRate;
        uint32_t delaySamps = static_cast<uint32_t>(samplesPerBeat * m_noteValue);
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            float inL = buffer.getReadPointer(0)[s];
            float inR = buffer.getReadPointer(1)[s];

            // 1. Fetch Delayed Output (Ping-Pong Cross-Tap)
            float outL = m_delayL.process(inR + m_feedbackL * m_lastOutR, delaySamps);
            float outR = m_delayR.process(inL + m_feedbackR * m_lastOutL, delaySamps);

            m_lastOutL = outL;
            m_lastOutR = outR;

            // 2. Mix
            buffer.getWritePointer(0)[s] = (inL * (1.0f - m_mix)) + (outL * m_mix);
            buffer.getWritePointer(1)[s] = (inR * (1.0f - m_mix)) + (outR * m_mix);
        }
    }

    void reset() noexcept override {
        m_delayL.reset();
        m_delayR.reset();
        m_lastOutL = 0.0f;
        m_lastOutR = 0.0f;
    }

    // Parameters
    void setNoteValue(float v) { m_noteValue = v; } // 0.25 (Quarter), 0.5 (Half), etc.
    void setFeedback(float f) { m_feedbackL = m_feedbackR = std::clamp(f, 0.0f, 0.99f); }
    void setMix(float m) { m_mix = m; }

private:
    double m_sampleRate = 44100.0;
    DelayLine m_delayL, m_delayR;
    float m_lastOutL = 0.0f;
    float m_lastOutR = 0.0f;
    
    float m_noteValue = 0.25f; // Quarter note sync
    float m_feedbackL = 0.5f;
    float m_feedbackR = 0.5f;
    float m_mix = 0.5f;
};

} // namespace Aura::DSP::Effects
