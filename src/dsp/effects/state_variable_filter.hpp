#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include "../../core/audio_buffer.hpp"

namespace Aura::DSP::Effects {

/**
 * @class StateVariableFilter
 * @brief Professional Zero-Delay Feedback (ZDF) SVF.
 * Support for Block-processing and legendary Pultec-style shelf extensions.
 */
class StateVariableFilter {
public:
    enum Type {
        LowPass, HighPass, BandPass, Bell, Notch, LowShelf, HighShelf
    };

    StateVariableFilter(double sr = 44100.0) : m_sampleRate(sr) {
        reset();
    }

    void setType(Type t) { m_type = t; }
    void prepareToPlay(double sr, uint32_t bs) { m_sampleRate = sr; }
    
    /**
     * @brief SET PARAMS: SVF Logic for Analog Modeling.
     */
    void setParams(float freq, float gainDb, float Q) {
        float g = std::tan(M_PI * freq / m_sampleRate);
        float k = 1.0f / Q;
        float a = std::pow(10.0f, gainDb / 40.0f); // Half-gain for shelf sum logic
        
        m_g = g; m_k = k; m_gain = a;
        m_a1 = 1.0f / (1.0f + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
    }

    /**
     * @brief BLOCK PROCESS: High-performance buffer sum.
     */
    void process(Core::AudioBuffer& buffer) {
        uint32_t numSamples = buffer.getNumSamples();
        uint32_t numChannels = buffer.getNumChannels();

        for (uint32_t c = 0; c < numChannels; ++c) {
            float* samples = buffer.getWritePointer(c);
            float& s1 = m_s1[c];
            float& s2 = m_s2[c];
            
            for (uint32_t i = 0; i < numSamples; ++i) {
                float x = samples[i];
                float v3 = x - s2;
                float v1 = m_a1 * s1 + m_a2 * v3;
                float v2 = s2 + m_a2 * s1 + m_a3 * v3;
                
                s1 = 2.0f * v1 - s1;
                s2 = 2.0f * v2 - s2;
                
                // Pultec-style Parallel Summation for Shelves
                if (m_type == LowShelf) samples[i] = x + m_gain * v2;
                else if (m_type == HighShelf) samples[i] = x + m_gain * (x - m_k * v1 - v2);
                else if (m_type == LowPass) samples[i] = v2;
                else if (m_type == HighPass) samples[i] = x - m_k * v1 - v2;
                else samples[i] = v1; // Bandpass fallback
            }
        }
    }

    void reset() {
        m_s1.fill(0.0f);
        m_s2.fill(0.0f);
    }

private:
    double m_sampleRate;
    Type m_type = LowPass;
    float m_g, m_k, m_gain, m_a1, m_a2, m_a3;
    std::array<float, 2> m_s1, m_s2; // Max stereo
};

} // namespace Aura::DSP::Effects
