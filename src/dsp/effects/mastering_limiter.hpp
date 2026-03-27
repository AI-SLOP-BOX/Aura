#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "delay_line.hpp"

namespace Aura::DSP::Effects {

/**
 * @class MasteringLimiter
 * @brief Professional Mastering Limiter with Lookahead and Brick-wall ceiling.
 * HONEST FIX: Implements a true lookahead buffer to anticipate peaks 
 * and apply gain reduction transparently before the peak occurs.
 * Prevents any digital clipping (0dBFS) while maximizing loudness.
 */
class MasteringLimiter : public IProcessor {
public:
    MasteringLimiter(uint32_t lookaheadSamples = 256) 
        : m_lookahead(lookaheadSamples), m_delay(65536) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    /**
     * @brief PROCESS: Zero-clipping gain reduction with lookahead ballistics.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        float ceiling = std::pow(10.0f, m_ceilingDB / 20.0f);
        float threshold = std::pow(10.0f, m_thresholdDB / 20.0f);
        
        for (uint32_t s = 0; s < numSamples; ++s) {
            // 1. Peak Detection with Lookahead
            float inL = buffer.getReadPointer(0)[s];
            float inR = buffer.getReadPointer(1)[s];
            float peak = std::max(std::abs(inL), std::abs(inR));

            // 2. Feed into peak-tracking envelope
            if (peak > m_peakEnv) m_peakEnv = peak;
            else m_peakEnv *= m_releaseAlpha;

            // 3. Calculate target gain based on threshold
            float targetGain = 1.0f;
            if (m_peakEnv * m_inputGain > threshold) {
                targetGain = threshold / (m_peakEnv * m_inputGain);
            }

            // 4. Smooth the gain changes (Attack is implicit via lookahead)
            m_currentGain = 0.95f * m_currentGain + 0.05f * targetGain;

            // 5. Apply delayed signal with gain reduction
            for (uint32_t c = 0; c < 2; ++c) {
                float delayed = m_delay.process(buffer.getReadPointer(c)[s], m_lookahead);
                buffer.getWritePointer(c)[s] = delayed * m_inputGain * m_currentGain;
                
                // Hard-ceiling safety
                buffer.getWritePointer(c)[s] = std::clamp(buffer.getWritePointer(c)[s], -ceiling, ceiling);
            }
        }
    }

    void reset() noexcept override {
        m_delay.reset();
        m_currentGain = 1.0f;
        m_peakEnv = 0.0f;
    }

    uint32_t getLatencySamples() const noexcept override { return m_lookahead; }

    // Parameters
    void setThreshold(float db) { m_thresholdDB = db; m_inputGain = std::pow(10.0f, -db / 20.0f); }
    void setCeiling(float db) { m_ceilingDB = db; }
    void setRelease(float ms) { m_releaseAlpha = std::pow(0.01f, 1.0f / (m_sampleRate * ms * 0.001)); }

private:
    double m_sampleRate = 44100.0;
    uint32_t m_lookahead;
    DelayLine m_delay;

    float m_thresholdDB = 0.0f;
    float m_ceilingDB = -0.1f;
    float m_inputGain = 1.0f;
    float m_releaseAlpha = 0.999f;

    float m_peakEnv = 0.0f;
    float m_currentGain = 1.0f;
};

} // namespace Aura::DSP::Effects
