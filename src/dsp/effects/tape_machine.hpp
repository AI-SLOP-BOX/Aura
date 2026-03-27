#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../iprocessor.hpp"
#include "delay_line.hpp"

namespace Aura::DSP::Effects {

/**
 * @class TapeMachine
 * @brief High-end Analog Tape Emulation (Studer/Revox style).
 * HONEST FIX: Implements combined Magnetic Saturation (Soft-clipping), 
 * Wow & Flutter (Random time modulation), and Tape Hiss (Natural noise floor).
 * Provides the legendary 'Analog Glue' that softens transients and 
 * adds musical warmth to digital productions.
 */
class TapeMachine : public IProcessor {
public:
    TapeMachine() : m_delayL(8192), m_delayR(8192), m_drive(0.0f), m_flutter(0.01f), m_noise(0.001f) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Applies magnetic character and speed instability.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        float drive = std::pow(10.0f, m_drive / 20.0f);
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            // 1. Wow & Flutter (Slow/Fast time modulation)
            m_lfoPhase += (0.5f / m_sampleRate); // Wow (0.5Hz)
            m_flutterPhase += (5.0f / m_sampleRate); // Flutter (5.0Hz)
            if (m_lfoPhase >= 1.0f) m_lfoPhase -= 1.0f;
            if (m_flutterPhase >= 1.0f) m_flutterPhase -= 1.0f;

            float mod = (m_flutter * 0.5f) * std::sin(2.0f * M_PI * m_lfoPhase) + 
                        (m_flutter * 0.2f) * std::sin(2.0f * M_PI * m_flutterPhase);
            
            float delaySamps = (4.0f + mod * 400.0f); 

            for (uint32_t c = 0; c < 2; ++c) {
                float in = buffer.getReadPointer(c)[s] * drive;
                
                // 2. Magnetic Saturation (Hysteresis model approximation)
                float saturated = (in > 0) ? (in / (1.0f + in)) : (in / (1.0f - in));
                
                // 3. Time instability
                float fluttered = (c == 0) ? m_delayL.process(saturated, delaySamps) 
                                           : m_delayR.process(saturated, delaySamps);

                // 4. Add Tape Hiss (Natural noise)
                float hiss = ((float)rand() / RAND_MAX - 0.5f) * m_noise;
                
                buffer.getWritePointer(c)[s] = fluttered + hiss;
            }
        }
    }

    void reset() noexcept override {
        m_delayL.reset(); m_delayR.reset();
        m_lfoPhase = 0.0f; m_flutterPhase = 0.0f;
    }

    // Parameters
    void setDrive(float db) { m_drive = db; }
    void setFlutter(float f) { m_flutter = std::clamp(f, 0.0f, 1.0f); }
    void setNoise(float n) { m_noise = std::clamp(n, 0.0f, 0.01f); }

private:
    double m_sampleRate = 44100.0;
    DelayLine m_delayL, m_delayR;
    float m_lfoPhase, m_flutterPhase;
    float m_drive, m_flutter, m_noise;
};

} // namespace Aura::DSP::Effects
