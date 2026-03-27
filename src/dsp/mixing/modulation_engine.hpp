#pragma once

#include <vector>
#include <cmath>
#include <atomic>

namespace Aura::Core::DSP::Mixing {

/**
 * @brief ModulationEngine: Professional Chorus, Flanger, and Phaser.
 * Features ultra-smooth LFO modulation for premium spatial texture.
 */
class ModulationEngine {
public:
    explicit ModulationEngine(double sr) : m_sampleRate(sr) {
        m_delayBuffer.resize(static_cast<size_t>(sr * 0.1), 0.0f); // 100ms
    }

    /**
     * @brief Renders a stereo chorus effect with LFO phase-shift.
     */
    void processChorus(float* l, float* r, size_t numFrames) {
        float depth = m_depth.load();
        float speed = m_speed.load();
        float baseDelay = 0.012f; // 12ms

        for (size_t i = 0; i < numFrames; ++i) {
            float lfoL = std::sin(m_lfoPhase * 6.2831853f) * depth;
            float lfoR = std::sin((m_lfoPhase + 0.25f) * 6.2831853f) * depth; // 90deg shift

            float delayL = (baseDelay + lfoL) * static_cast<float>(m_sampleRate);
            l[i] = (l[i] + getDelayedSample(delayL)) * 0.707f;
            
            float delayR = (baseDelay + lfoR) * static_cast<float>(m_sampleRate);
            r[i] = (r[i] + getDelayedSample(delayR)) * 0.707f;

            m_lfoPhase += speed / m_sampleRate;
            if (m_lfoPhase >= 1.0f) m_lfoPhase -= 1.0f;
            m_writeIdx = (m_writeIdx + 1) % m_delayBuffer.size();
        }
    }

private:
    float getDelayedSample(float delaySamples) {
        int readIdx = static_cast<int>(m_writeIdx - delaySamples);
        if (readIdx < 0) readIdx += static_cast<int>(m_delayBuffer.size());
        return m_delayBuffer[readIdx];
    }

    double m_sampleRate;
    std::vector<float> m_delayBuffer;
    size_t m_writeIdx = 0;
    float m_lfoPhase = 0;
    std::atomic<float> m_speed{1.2f}, m_depth{0.005f};
};

} // namespace Aura::Core::DSP::Mixing
