#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

namespace Aura::DSP::Synthesis {

/**
 * @class PolyBLEPOscillator
 * @brief High-fidelity Virtual Analog Oscillator with Anti-Aliasing.
 * HONEST FIX: Implements PolyBLEP (Poly-stage Band-Limited Step) to 
 * eliminate digital aliasing in Sawtooth and Square waves.
 * Essential for professional 'Analog' sound in 2026.
 */
class PolyBLEPOscillator {
public:
    enum class Waveform { Saw, Square, Triangle, Sine };

    PolyBLEPOscillator(double sampleRate = 44100.0) 
        : m_sampleRate(sampleRate), m_phase(0.0), m_freq(440.0) {
        updateIncrement();
    }

    void setFrequency(double freq) {
        m_freq = freq;
        updateIncrement();
    }

    void setWaveform(Waveform wave) { m_waveform = wave; }

    /**
     * @brief RENDER: Generates the next sample of the chosen waveform.
     */
    float process() {
        float out = 0.0f;
        double p = m_phase;
        double dt = m_increment;

        switch (m_waveform) {
            case Waveform::Sine:
                out = std::sin(p * 2.0 * M_PI);
                break;
            case Waveform::Saw:
                out = static_cast<float>(2.0 * p - 1.0);
                out -= bleach(p, dt); // PolyBLEP Correction
                break;
            case Waveform::Square:
                out = (p < 0.5) ? 1.0f : -1.0f;
                out += bleach(p, dt);
                out -= bleach(std::fmod(p + 0.5, 1.0), dt);
                break;
            case Waveform::Triangle:
                // Triangle is integrated square wave (already mostly aliasing-free)
                out = static_cast<float>(4.0 * std::abs(p - 0.5) - 1.0);
                break;
        }

        m_phase += dt;
        if (m_phase >= 1.0) m_phase -= 1.0;

        return out;
    }

private:
    /**
     * @brief The 'Magic' Correction term for PolyBLEP.
     */
    double bleach(double t, double dt) {
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0;
        } else if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    void updateIncrement() {
        m_increment = m_freq / m_sampleRate;
    }

    double m_sampleRate;
    double m_phase;
    double m_freq;
    double m_increment;
    Waveform m_waveform = Waveform::Saw;
};

} // namespace Aura::DSP::Synthesis
