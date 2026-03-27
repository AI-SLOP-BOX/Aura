#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../../core/engine/modulator_system.hpp"
#include "../effects/delay_line.hpp"

namespace Aura::DSP::Effects {

/**
 * @brief VocalDoubler: Industrial-standard vocal thickening.
 * Creates 'Double' takes automatically with micro-timing and pitch shifts.
 */
class VocalDoubler : public IProcessor {
public:
    VocalDoubler(double sr = 44100.0) : m_sampleRate(sr), m_lfo(sr) {
        m_lfo.setParameters(Core::Engine::LFO::Waveform::Sine, 0.2f, 0.005f);
        m_delayL.resize(static_cast<uint32_t>(sr * 0.1)); // 100ms max
        m_delayR.resize(static_cast<uint32_t>(sr * 0.1));
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        for (uint32_t i = 0; i < numSamples; ++i) {
            float inL = l[i];
            float inR = r[i];

            // 1. GENERATE MODULATION (Micro-detuning)
            float mod = m_lfo.getNextValue();
            
            // 2. DELAY L/R (Micro-timing offsets)
            float dL = 20.0f + mod * 10.0f; // 10ms - 30ms jitter
            float dR = 25.0f - mod * 12.0f;
            
            float voiceL = m_delayL.read(dL);
            float voiceR = m_delayR.read(dR);
            
            m_delayL.write(inL);
            m_delayR.write(inR);

            // 3. STEREO SUM (Center + Wide Doubles)
            l[i] = inL * 0.7f + voiceL * 0.5f;
            r[i] = inR * 0.7f + voiceR * 0.5f;
        }
    }

    void setSampleRate(double sr) override { m_sampleRate = sr; }
    uint32_t getLatency() const override { return 0; }

private:
    double m_sampleRate;
    Core::Engine::LFO m_lfo;
    DelayLine m_delayL, m_delayR;
};

} // namespace Aura::DSP::Effects
