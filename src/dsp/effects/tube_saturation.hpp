#pragma once

#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class TubeSaturation
 * @brief Professional Vacuum Tube Emulation (Analog Warmth).
 * HONEST FIX: Implements a non-linear transfer function (Asymmetrical soft-clipping) 
 * to generate even-order harmonics characteristic of Triode and Pentode tubes.
 * It adds 'Glow' and 'Weight' to digital tracks without harsh digital clipping.
 */
class TubeSaturation : public IProcessor {
public:
    TubeSaturation() : m_drive(0.0f), m_bias(0.0f), m_dryWet(1.0f) {}

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        // No special prep needed for stateless waveshaping
    }

    /**
     * @brief PROCESS: Applies the non-linear transfer function.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        float drive = std::pow(10.0f, m_drive / 20.0f);
        
        for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
            float* p = buffer.getWritePointer(c);
            for (uint32_t s = 0; s < numSamples; ++s) {
                float in = p[s] * drive + m_bias;
                
                // 1. Asymmetrical Soft Clipping (Tube Characteristic)
                // f(x) = x / (1 + |x|) or similar sigmoid
                float saturated = (in > 0) ? (in / (1.0f + in)) : (in / (1.0f - in));
                
                // 2. DC Offset Compensation (Simplified)
                saturated -= m_bias * 0.5f;

                // 3. Dry/Wet Mix
                p[s] = (p[s] * (1.0f - m_dryWet)) + (saturated * m_dryWet);
            }
        }
    }

    void reset() noexcept override {}

    // Parameters
    void setDrive(float db) { m_drive = db; }
    void setBias(float b) { m_bias = b; }
    void setDryWet(float mix) { m_dryWet = mix; }

private:
    float m_drive;   // Gain in dB
    float m_bias;    // Asymmetry bias
    float m_dryWet;  // 0.0 to 1.0
};

} // namespace Aura::DSP::Effects
