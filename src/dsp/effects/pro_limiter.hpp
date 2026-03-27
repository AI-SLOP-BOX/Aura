#include <cmath>
#include <algorithm>
#include <array>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class ProLimiter
 * @brief Professional Brickwall Lookahead Limiter.
 */
class ProLimiter : public IProcessor {
public:
    static constexpr uint32_t kLookahead = 480; 

    ProLimiter(double sr = 44100.0) : m_sampleRate(sr) {
        m_delayL.fill(0.0f);
        m_delayR.fill(0.0f);
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        m_release = std::exp(-1.0f / (0.05f * static_cast<float>(sr))); // 50ms default
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        uint32_t numSamples = buffer.getNumSamples();
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);

        for (uint32_t s = 0; s < numSamples; ++s) {
            float inL = l[s];
            float inR = r[s];
            
            // 1. Peak Detection (Link)
            float peak = std::max(std::abs(inL), std::abs(inR));
            float targetGain = (peak > m_threshold) ? (m_threshold / peak) : 1.0f;

            // 2. Lookahead Ballistics (Attack must be faster than lookahead)
            if (targetGain < m_envelope) {
                m_envelope = targetGain; // Instant attack on peak
            } else {
                m_envelope = targetGain + m_release * (m_envelope - targetGain);
            }

            // 3. Apply to delayed signal
            uint32_t readIdx = (m_writeIdx - kLookahead + m_delayL.size()) % m_delayL.size();
            l[s] = m_delayL[readIdx] * m_envelope;
            r[s] = m_delayR[readIdx] * m_envelope;

            // 4. Update Delay Lines
            m_delayL[m_writeIdx] = inL;
            m_delayR[m_writeIdx] = inR;
            m_writeIdx = (m_writeIdx + 1) % m_delayL.size();
        }
    }

    void reset() noexcept override {
        m_envelope = 1.0f;
        m_writeIdx = 0;
        m_delayL.fill(0.0f);
        m_delayR.fill(0.0f);
    }

    void setThreshold(float db) { m_threshold = std::pow(10.0f, db / 20.0f); }

private:
    double m_sampleRate;
    float m_threshold = 1.0f;
    float m_envelope = 1.0f;
    float m_release = 0.999f;
    std::array<float, 1024> m_delayL, m_delayR;
    uint32_t m_writeIdx = 0;
};

} // namespace Aura::DSP::Effects
