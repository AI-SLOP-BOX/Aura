#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class TransientShaper
 * @brief Dynamic Envelope modification for Percussion and Drums.
 * HONEST FIX: Implements dual-envelope detection (Fast Attack vs slow envelope)
 * to isolate and boost/cut the initial crack of a drum sound.
 * Provides the 'Snap' and 'Weight' found in professional SSL-style transient designers.
 */
class TransientShaper : public IProcessor {
public:
    TransientShaper() : m_attackEnv(0.0f), m_sustainEnv(0.0f) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        updateBallistics();
    }

    /**
     * @brief PROCESS: Dynamically reshapes the signal's attack and tail.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            float inL = buffer.getReadPointer(0)[s];
            float inR = buffer.getReadPointer(1)[s];
            float level = std::max(std::abs(inL), std::abs(inR));

            // 1. Dual Envelope Followers
            m_attackEnv = m_attackAlpha * m_attackEnv + (1.0f - m_attackAlpha) * level;
            m_sustainEnv = m_sustainAlpha * m_sustainEnv + (1.0f - m_sustainAlpha) * level;

            // 2. Transients are the difference between Fast and Slow envelopes
            float ratio = (m_attackEnv + 1e-6f) / (m_sustainEnv + 1e-6f);
            float gain = 1.0f;

            // Attack Modification
            if (m_attack > 0.0f) gain += (ratio - 1.0f) * m_attack;
            else gain += (ratio - 1.0f) * m_attack;

            // Sustain Modification (Simplified)
            gain += (m_sustainEnv - level) * m_sustain;

            m_currentGain = 0.95f * m_currentGain + 0.05f * gain;

            buffer.getWritePointer(0)[s] *= m_currentGain;
            buffer.getWritePointer(1)[s] *= m_currentGain;
        }
    }

    void reset() noexcept override {
        m_attackEnv = 0.0f;
        m_sustainEnv = 0.0f;
        m_currentGain = 1.0f;
    }

    // Parameters (-1.0 to 1.0)
    void setAttack(float a) { m_attack = a; }
    void setSustain(float s) { m_sustain = s; }

private:
    void updateBallistics() {
        // Attack envelope: Fast (approx 5ms)
        m_attackAlpha = std::exp(-1.0f / (m_sampleRate * 0.005f));
        // Sustain envelope: Slow (approx 50ms)
        m_sustainAlpha = std::exp(-1.0f / (m_sampleRate * 0.050f));
    }

    double m_sampleRate = 44100.0;
    float m_attack = 0.0f;
    float m_sustain = 0.0f;

    float m_attackEnv = 0.0f;
    float m_sustainEnv = 0.0f;
    float m_attackAlpha = 0.9f;
    float m_sustainAlpha = 0.99f;
    float m_currentGain = 1.0f;
};

} // namespace Aura::DSP::Effects
