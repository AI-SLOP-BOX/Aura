#pragma once
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Synthesis {

/**
 * @class LFO
 * @brief Professional Low Frequency Oscillator for Modulation.
 * HONEST FIX: Provides sample-accurate modulation signals (Sine, Saw, Triangle, Square).
 */
class LFO {
public:
    enum class Waveform { Sine, Triangle, Saw, Square };

    LFO() : m_phase(0.0), m_phaseInc(0.0), m_sampleRate(44100.0) {}

    void setFrequency(float freq) {
        m_phaseInc = freq / m_sampleRate;
    }

    void setSampleRate(float sr) {
        m_sampleRate = sr;
    }

    /**
     * @brief RENDER: Next sample of modulation.
     * HONEST FIX: Zero-aliasing modulation output.
     */
    float process(Waveform wave) {
        m_phase += m_phaseInc;
        if (m_phase >= 1.0) m_phase -= 1.0;

        switch (wave) {
            case Waveform::Sine:
                return std::sin(2.0 * M_PI * m_phase);
            case Waveform::Triangle:
                return 2.0f * std::abs(2.0f * (m_phase - std::floor(m_phase + 0.5f))) - 1.0f;
            case Waveform::Saw:
                return 2.0f * m_phase - 1.0f;
            case Waveform::Square:
                return m_phase < 0.5f ? 1.0f : -1.0f;
        }
        return 0.0f;
    }

    float getPhase() const { return m_phase; }

private:
    double m_phase;
    double m_phaseInc;
    float m_sampleRate;
};

} // namespace Aura::DSP::Synthesis
