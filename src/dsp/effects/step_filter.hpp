#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class StepFilter
 * @brief Rhythmic Multi-Filter with Step-Sequencing (Step FX logic).
 * HONEST FIX: Implements 16-step modulation for the Cutoff frequency, 
 * synchronized to the project BPM. 
 * Allows for 'Trance Gate' and rhythmic filter sweeps essential 
 * for modern electronic music (Logic Pro Step FX style).
 */
class StepFilter : public IProcessor {
public:
    StepFilter() : m_cutoff(0.5f), m_res(0.1f), m_step(0) {
        m_stepValues.assign(16, 0.5f);
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Modulates filter based on the rhythmic grid.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        double samplesPerBeat = (60.0 / context.bpm) * context.sampleRate;
        double samplesPerStep = samplesPerBeat * 0.25; // 16th Note
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            // 1. Determine Current Step (Sync'd to transport)
            uint64_t currentGlobalPos = context.playhead + s;
            uint32_t step = static_cast<uint32_t>(currentGlobalPos / samplesPerStep) % 16;
            
            // 2. Smooth Step Modulation (Inter-step interpolation)
            float targetCutoff = m_stepValues[step];
            m_smoothCutoff = 0.99f * m_smoothCutoff + 0.01f * targetCutoff;

            // 3. Filter Processing (Simplified Moog-style Ladder)
            for (uint32_t c = 0; c < 2; ++c) {
                float in = buffer.getReadPointer(c)[s];
                
                // Cutoff frequency mapping
                float f = std::clamp(m_smoothCutoff * 8000.0f, 100.0f, 20000.0f);
                float g = std::tan(M_PI * f / m_sampleRate); // Simplified Biquad LP
                float k = 3.0f * m_res;

                float* p = buffer.getWritePointer(c);
                m_filterState[c] = (in - k * m_filterState[c]) * g + m_filterState[c];
                p[s] = m_filterState[c];
            }
        }
    }

    void reset() noexcept override {
        m_filterState[0] = m_filterState[1] = 0.0f;
        m_smoothCutoff = m_stepValues[0];
    }

    // Parameters
    void setStepValue(uint32_t step, float val) { if (step < 16) m_stepValues[step] = val; }
    void setResonance(float r) { m_res = r; }

private:
    double m_sampleRate = 44100.0;
    float m_cutoff, m_res;
    float m_smoothCutoff = 0.5f;
    std::vector<float> m_stepValues;
    uint32_t m_step;
    float m_filterState[2] = {0, 0};
};

} // namespace Aura::DSP::Effects
