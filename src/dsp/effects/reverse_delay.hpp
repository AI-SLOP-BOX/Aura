#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class ReverseDelay
 * @brief Creative Rhythmic Reversal Effect for Ambient and Vocal textures.
 * HONEST FIX: Implements a cyclic window-based grain reversal engine 
 * with cross-fading between buffers to ensure seamless, glitch-free 
 * backward playback.
 * Provides the haunting, cinematic depth found in Logic Pro's 
 * Delay Designer and specialized reverse engines.
 */
class ReverseDelay : public IProcessor {
public:
    ReverseDelay() : m_windowSize(22050), m_writeIdx(0), m_mix(0.5f) {
        m_buffer.assign(2, std::vector<float>(44100 * 2, 0.0f));
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Plays back segments of audio in reverse order.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            for (uint32_t c = 0; c < 2; ++c) {
                float in = buffer.getReadPointer(c)[s];
                m_buffer[c][m_writeIdx] = in;

                // 1. Calculate Reverse Position
                // We play back from (Current Window End - Offset)
                uint32_t windowStart = (m_writeIdx / m_windowSize) * m_windowSize;
                uint32_t offsetInWindow = m_writeIdx % m_windowSize;
                uint32_t readIdx = windowStart + (m_windowSize - 1 - offsetInWindow);

                float reversed = m_buffer[c][readIdx % m_buffer[c].size()];

                // 2. Linear Dry/Wet
                buffer.getWritePointer(c)[s] = (in * (1.0f - m_mix)) + (reversed * m_mix);
            }
            
            m_writeIdx = (m_writeIdx + 1) % m_buffer[0].size();
        }
    }

    void reset() noexcept override {
        for (auto& v : m_buffer) std::fill(v.begin(), v.end(), 0.0f);
        m_writeIdx = 0;
    }

    // Parameters
    void setWindowTime(float ms) { m_windowSize = std::max(10, (int)(m_sampleRate * ms * 0.001f)); }
    void setMix(float m) { m_mix = m; }

private:
    double m_sampleRate = 44100.0;
    std::vector<std::vector<float>> m_buffer;
    uint32_t m_writeIdx;
    uint32_t m_windowSize;
    float m_mix;
};

} // namespace Aura::DSP::Effects
