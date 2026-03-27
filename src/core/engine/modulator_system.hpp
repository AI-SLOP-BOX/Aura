#pragma once

#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

namespace Aura::Core::Engine {

/**
 * @brief LFO: Low-Frequency Oscillator for rhythmic modulation.
 */
class LFO {
public:
    enum class Waveform { Sine, Triangle, SawUp, SawDown, Square, RandomSH };

    LFO(double sr = 44100.0) : m_sampleRate(sr) {}

    void setParameters(Waveform wave, float freq, float depth, float phaseOffset = 0.0f) {
        m_wave = wave;
        m_freq = freq;
        m_depth = depth;
        m_phase = phaseOffset;
    }

    /**
     * @brief Generates the next modulation value [-1.0, 1.0] * depth.
     */
    float getNextValue() {
        float out = 0.0f;
        switch (m_wave) {
            case Waveform::Sine: 
                out = std::sin(m_phase * 2.0f * M_PI); 
                break;
            case Waveform::Triangle: 
                out = 2.0f * std::abs(2.0f * (m_phase - std::floor(m_phase + 0.5f))) - 1.0f; 
                break;
            case Waveform::SawUp: 
                out = 2.0f * (m_phase - std::floor(m_phase + 0.5f)); 
                break;
            case Waveform::SawDown: 
                out = -2.0f * (m_phase - std::floor(m_phase + 0.5f)); 
                break;
            case Waveform::Square: 
                out = (m_phase < 0.5f) ? 1.0f : -1.0f; 
                break;
            case Waveform::RandomSH:
                // (Conceptual Sample & Hold)
                if (m_phase < m_lastPhase) out = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
                else out = m_lastVal;
                break;
        }
        
        m_lastVal = out;
        m_lastPhase = m_phase;
        m_phase += m_freq / m_sampleRate;
        if (m_phase >= 1.0f) m_phase -= 1.0f;
        
        return out * m_depth;
    }

private:
    double m_sampleRate;
    Waveform m_wave = Waveform::Sine;
    float m_freq = 1.0f, m_depth = 1.0f, m_phase = 0.0f;
    float m_lastPhase = 0.0f, m_lastVal = 0.0f;
};

/**
 * @brief ModulatorSystem: Central hub for rhythmic parameter automation.
 */
class ModulatorSystem {
public:
    struct Mapping {
        uint32_t targetParamId;
        float amount;
    };

    void update(uint32_t numSamples) {
        for (auto& lfo : m_lfos) {
            float mod = lfo.lfo.getNextValue();
            for (const auto& m : lfo.mappings) {
                // (Actual parameter tree injection logic)
            }
        }
    }

private:
    struct LFOInstance {
        LFO lfo;
        std::vector<Mapping> mappings;
    };
    std::vector<LFOInstance> m_lfos;
};

} // namespace Aura::Core::Engine
