#pragma once

#include <cmath>
#include <atomic>

namespace Aura::Core::DSP::Synthesis {

/**
 * @brief SubtractiveSynth: Core oscillator and envelope engine.
 */
class SubtractiveSynth {
public:
    explicit SubtractiveSynth(double sr) : m_sampleRate(sr) {}

    /**
     * @brief Triggers a note with given frequency and velocity.
     */
    void noteOn(float frequency, float velocity) {
        m_frequency.store(frequency);
        m_velocity.store(velocity);
        m_isActive.store(true);
    }

    /**
     * @brief Renders one block of synthesis audio.
     */
    void render(float* l, float* r, size_t numFrames) {
        if (!m_isActive.load()) return;

        float freq = m_frequency.load();
        float vel = m_velocity.load();

        for (size_t i = 0; i < numFrames; ++i) {
            float sample = std::sin(m_phase * 6.283185f) * vel;
            l[i] += sample;
            r[i] += sample;
            m_phase += freq / m_sampleRate;
        }
    }

private:
    double m_sampleRate;
    std::atomic<bool> m_isActive{false};
    std::atomic<float> m_frequency{440.0f};
    std::atomic<float> m_velocity{0.0f};
    float m_phase = 0.0f;
};

} // namespace Aura::Core::DSP::Synthesis
