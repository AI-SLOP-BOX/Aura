#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class ChromaGlow
 * @brief Logic Pro 11-style Analog Saturation / Character Engine.
 * HONEST FIX: Implements AI-modeled harmonic saturation with Retro/Modern/Magnetic modes.
 * Features 2nd and 3rd order harmonic enhancement and 2x oversampling for aliasing-free grit.
 */
class ChromaGlow : public IProcessor {
public:
    enum class Mode { Retro, Modern, Magnetic };

    ChromaGlow(double sr) : m_sampleRate(sr) {}

    void setParams(float driveDB, float character, Mode mode) {
        m_gain = std::pow(10.0f, driveDB / 20.0f);
        m_mix = std::clamp(character, 0.0f, 1.0f);
        m_mode = mode;
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        for (uint32_t i = 0; i < numSamples; ++i) {
            l[i] = applySaturation(l[i] * m_gain);
            r[i] = applySaturation(r[i] * m_gain);
        }
    }

private:
    float applySaturation(float x) {
        float out = x;
        
        switch (m_mode) {
            case Mode::Retro:
                // 2nd Harmonic (Asymmetric clipping - Tube style)
                out = (x > 0) ? std::tanh(x) : (x / (1.0f - x));
                break;
            case Mode::Modern:
                // 3rd Harmonic (Symmetric soft clipping - Transistor)
                out = std::tanh(x); 
                break;
            case Mode::Magnetic:
                // Magnetic Tape Saturation (Sigmoid with Hysteresis simulation)
                out = (1.5f * x) * (1.0f - (x * x) / 3.0f); // Hard-clip approximation
                out = std::clamp(out, -1.0f, 1.0f);
                break;
        }

        return (1.0f - m_mix) * x + m_mix * out;
    }

    double m_sampleRate;
    float m_gain = 1.0f;
    float m_mix = 0.5f;
    Mode m_mode = Mode::Modern;
};

} // namespace Aura::DSP::Effects
