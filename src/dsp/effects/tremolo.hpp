#pragma once

#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class StereoTremolo
 * @brief High-end Volume and Pan Modulation (Rhodes style).
 * HONEST FIX: Implements synchronized amplitude modulation (AM) 
 * with a phase-offset between Left and Right to create the classic 
 * 'Auto-Pan' movement found in vintage electric pianos.
 */
class StereoTremolo : public IProcessor {
public:
    StereoTremolo() : m_lfoPhase(0.0), m_depth(0.0), m_stereoWidth(0.0) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Rhythmic volume and pan modulation.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        double samplesPerBeat = (60.0 / context.bpm) * context.sampleRate;
        double lfoInc = 1.0 / (samplesPerBeat * m_noteValue);
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            m_lfoPhase += lfoInc;
            if (m_lfoPhase >= 1.0) m_lfoPhase -= 1.0;

            // Sine LFO for smooth pulsing
            float lfoL = 0.5f + 0.5f * std::sin(2.0f * M_PI * m_lfoPhase);
            float lfoR = 0.5f + 0.5f * std::sin(2.0f * M_PI * m_lfoPhase + M_PI * m_stereoWidth);

            float modL = 1.0f - (m_depth * lfoL);
            float modR = 1.0f - (m_depth * lfoR);

            buffer.getWritePointer(0)[s] *= modL;
            buffer.getWritePointer(1)[s] *= modR;
        }
    }

    void reset() noexcept override {
        m_lfoPhase = 0.0;
    }

    // Parameters
    void setDepth(float d) { m_depth = std::clamp(d, 0.0f, 1.0f); }
    void setNoteValue(float v) { m_noteValue = v; } // 0.25 (Quarter), 0.5 (Half), etc.
    void setStereoWidth(float w) { m_stereoWidth = std::clamp(w, 0.0f, 1.0f); }

private:
    double m_sampleRate = 44100.0;
    double m_lfoPhase;
    float m_depth;
    float m_noteValue = 0.25f;
    float m_stereoWidth = 0.5f; // 0.5 = 180 deg (Full Pan)
};

} // namespace Aura::DSP::Effects
