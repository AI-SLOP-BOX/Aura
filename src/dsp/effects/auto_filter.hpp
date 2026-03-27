#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../utils/dsp_utils.hpp"

namespace Aura::DSP::Effects {

/**
 * @class AutoFilter
 * @brief Professional Dynamic Resonant Filter (Auto-Wah).
 */
class AutoFilter : public IProcessor {
public:
    AutoFilter() : m_cutoffBase(0.2f), m_res(0.3f), m_sens(0.8f), m_env(0.0f) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        updateTimeConstants();
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            float inL = buffer.getReadPointer(0)[s];
            float inR = buffer.getReadPointer(1)[s];
            float mid = (inL + inR) * 0.5f;

            // 1. Envelope Follower (Correct timing)
            float absIn = std::abs(mid);
            if (absIn > m_env) m_env += (absIn - m_env) * m_attack;
            else m_env += (absIn - m_env) * m_release;

            // 2. Sample-Accurate Modulated Cutoff
            float depth = m_env * m_sens;
            float targetCutoff = std::clamp(m_cutoffBase + depth, 0.02f, 0.98f);
            
            // Continuous g-coefficient smoothing (No more clicking)
            float f = targetCutoff * 8000.0f; 
            float targetG = std::tan(3.1415926535 * f / m_sampleRate);
            m_g += (targetG - m_g) * 0.2f; // Smooth ramp
            m_k = 2.0f - (m_res * 1.95f);

            // 3. SVF Execution (Stereo)
            for (uint32_t c = 0; c < 2; ++c) {
                float in = buffer.getReadPointer(c)[s];
                float hp = (in - m_k * m_s1[c] - m_s2[c]) / (1.0f + m_k * m_g + m_g * m_g);
                float bp = m_g * hp + m_s1[c];
                float lp = m_g * bp + m_s2[c];

                m_s1[c] = m_g * hp + bp;
                m_s2[c] = m_g * bp + lp;
                buffer.getWritePointer(c)[s] = lp;
            }
        }
    }

    void reset() noexcept override {
        m_env = 0.0f;
        m_s1[0] = m_s1[1] = 0.0f;
        m_s2[0] = m_s2[1] = 0.0f;
    }

    void setParameter(uint32_t id, float value) noexcept override {
        if (id == 0) m_cutoffBase = std::clamp(value, 0.0f, 1.0f);
        else if (id == 1) m_res = std::clamp(value, 0.0f, 1.0f);
        else if (id == 2) m_sens = std::clamp(value, 0.0f, 1.0f);
    }

private:
    void updateTimeConstants() {
        m_attack = 1.0f - std::exp(-1.0f / (0.005f * m_sampleRate)); // 5ms
        m_release = 1.0f - std::exp(-1.0f / (0.100f * m_sampleRate)); // 100ms
    }

    double m_sampleRate = 44100.0;
    float m_cutoffBase, m_res, m_sens;
    float m_env, m_attack, m_release;
    float m_g = 0, m_k = 0;
    float m_s1[2] = {0,0}, m_s2[2] = {0,0}; // Filter states
};

} // namespace Aura::DSP::Effects
