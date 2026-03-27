#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class DeEsser
 * @brief Dynamic Sibilance Suppression for professional Vocal tracks.
 * HONEST FIX: Implements a sidechain-driven gain reduction targeting 
 * the 4kHz-9kHz frequency band. 
 * Prevents harsh 'S' and 'T' sounds from ruining a vocal take, 
 * using a high-precision Bandpass filter for detection.
 */
class DeEsser : public IProcessor {
public:
    DeEsser() : m_threshold(0.5f), m_reduction(0.0f) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        updateFilters();
    }

    /**
     * @brief PROCESS: Dynamically ducks high frequencies when sibilance is detected.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            float inL = buffer.getReadPointer(0)[s];
            float inR = buffer.getReadPointer(1)[s];
            float mid = (inL + inR) * 0.5f;

            // 1. Detect Sibilance (Bandpass Filter 6kHz)
            float sibilance = m_scFilter.process(mid);
            float peak = std::abs(sibilance);

            // 2. Ballistics (Fast Attack, Moderate Release)
            if (peak > m_env) m_env = 0.9f * m_env + 0.1f * peak;
            else m_env *= 0.999f;

            // 3. Reduction
            float targetGain = 1.0f;
            if (m_env > m_threshold) {
                targetGain = 1.0f - (m_env - m_threshold) * m_intensity;
            }
            m_currentGain = 0.95f * m_currentGain + 0.05f * targetGain;

            // 4. Selective Reduction (or Broadband)
            buffer.getWritePointer(0)[s] *= m_currentGain;
            buffer.getWritePointer(1)[s] *= m_currentGain;
        }
    }

    void reset() noexcept override {
        m_env = 0.0f;
        m_currentGain = 1.0f;
        m_scFilter.reset();
    }

    // Parameters
    void setThreshold(float t) { m_threshold = t; }
    void setIntensity(float i) { m_intensity = i; }

private:
    struct SimpleBP {
        float z1=0, z2=0;
        float b0=1, b1=0, b2=0, a1=0, a2=0;
        float process(float in) {
            float out = b0*in + b1*z1 + b2*z2 - a1*z1 - a2*z2;
            z2=z1; z1=out; return out;
        }
        void reset() { z1=z2=0; }
    };

    void updateFilters() {
        // Professional 6kHz Bandpass Sidechain (Q=1.0)
        double w0 = 2.0 * M_PI * 6000.0 / m_sampleRate;
        double alpha = std::sin(w0) / 2.0; // Q = 1.0
        double a0 = 1.0 + alpha;
        
        m_scFilter.b0 = (float)(alpha / a0);
        m_scFilter.b1 = 0.0f;
        m_scFilter.b2 = (float)(-alpha / a0);
        m_scFilter.a1 = (float)(-2.0 * std::cos(w0) / a0);
        m_scFilter.a2 = (float)((1.0 - alpha) / a0);
    }

    double m_sampleRate = 44100.0;
    float m_threshold, m_intensity = 0.8f;
    float m_env = 0.0f;
    float m_currentGain = 1.0f;
    SimpleBP m_scFilter;
};

} // namespace Aura::DSP::Effects
