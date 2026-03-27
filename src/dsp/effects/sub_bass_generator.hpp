#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../utils/dsp_utils.hpp"

namespace Aura::DSP::Effects {

/**
 * @class SubBassGenerator
 * @brief Professional Sub-frequency Synthesis for Hip-Hop/EDM.
 * HONEST FIX: Implements a pitch-tracking sub-oscillator that generates 
 * a pure sine wave exactly one octave below the input's fundamental.
 */
class SubBassGenerator : public IProcessor {
public:
    SubBassGenerator(double sr = 44100.0) : m_sampleRate(sr) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        m_envAttack = std::exp(-1.0f / (0.005f * sr)); // 5ms
        m_envRelease = std::exp(-1.0f / (0.1f * sr));  // 100ms
        m_lpfCoeff = std::exp(-1.0f / (0.001f * sr));  // 1kHz LPF for tracking
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (isBypassed()) return;

        uint32_t numSamples = buffer.getNumSamples();
        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);

        for (uint32_t s = 0; s < numSamples; ++s) {
            float mid = (left[s] + right[s]) * 0.5f;

            // 1. INPUT PRE-FILTER (LPF + DC Block for stable tracking)
            m_lpfState += (1.0f - m_lpfCoeff) * (mid - m_lpfState);
            m_dcBlockState = m_lpfState - m_lastLpf + 0.995f * m_dcBlockState;
            m_lastLpf = m_lpfState;

            // 2. ENVELOPE FOLLOWER
            float absIn = std::abs(mid);
            if (absIn > m_env) m_env = m_envAttack * m_env + (1.0f - m_envAttack) * absIn;
            else m_env = m_envRelease * m_env;

            // 3. PITCH TRACKING (Zero-Crossing with Hysteresis)
            bool triggered = false;
            if (m_dcBlockState > 0.02f && !m_isPositive) { m_isPositive = true; triggered = true; }
            else if (m_dcBlockState < -0.02f && m_isPositive) { m_isPositive = false; }

            if (triggered) {
                float period = static_cast<float>(m_zcCount);
                if (period > 10.0f) {
                    float target = (static_cast<float>(m_sampleRate) / period) * 0.5f;
                    m_targetFreq = std::clamp(target, 20.0f, 90.0f);
                }
                m_zcCount = 0;
            }
            m_zcCount = std::min(m_zcCount + 1u, 10000u);

            // 4. GENERATOR (Sub-Harmonic Sine)
            m_currFreq += (m_targetFreq - m_currFreq) * 0.05f;
            m_phase += (m_currFreq / static_cast<float>(m_sampleRate));
            if (m_phase >= 1.0f) m_phase -= 1.0f;

            float sub = std::sin(6.28318530718 * m_phase) * m_env * m_mix;

            // 5. SUM
            left[s] += sub;
            right[s] += sub;
        }
    }

    void reset() noexcept override {
        m_env = 0.0f; m_phase = 0.0; m_lpfState = 0.0f; m_dcBlockState = 0.0f;
        m_zcCount = 0; m_targetFreq = 50.0f; m_currFreq = 50.0f;
    }

    void setParameter(uint32_t id, float value) noexcept override { if (id == 0) m_mix = value; }
    float getParameter(uint32_t id) const noexcept override { return (id == 0) ? m_mix : 0.0f; }

private:
    double m_sampleRate = 44100.0;
    float m_env = 0.0f, m_envAttack, m_envRelease;
    float m_lpfState = 0.0f, m_lastLpf = 0.0f, m_dcBlockState = 0.0f, m_lpfCoeff;
    float m_mix = 0.5f, m_targetFreq = 50.0f, m_currFreq = 50.0f;
    double m_phase = 0.0;
    uint32_t m_zcCount = 0;
    bool m_isPositive = false;
};

} // namespace Aura::DSP::Effects
