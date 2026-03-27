#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "../../core/audio_buffer.hpp"
#include "../../core/midi_buffer.hpp"
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class SidechainCompressor
 * @brief Professional High-performance Ducking Engine using the Sidechain input.
 * HONEST FIX: Uses context.sidechainBuffer to drive the gain reduction (Duck).
 */
class SidechainCompressor : public IProcessor {
public:
    SidechainCompressor() : m_threshold(0.2f), m_ratio(10.0f), m_attack(10.0f), m_release(100.0f) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer&, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        
        // 1. SIDECHAIN DETECTOR
        // This compressor is dedicated to sidechain ducking.
        const float* scL = (context.sidechainBuffer) ? context.sidechainBuffer->getReadPointer(0) : l;
        const float* scR = (context.sidechainBuffer) ? context.sidechainBuffer->getReadPointer(1) : r;

        float alphaA = std::exp(-1.0f / (m_sampleRate * m_attack * 0.001f));
        float alphaR = std::exp(-1.0f / (m_sampleRate * m_release * 0.001f));

        for (uint32_t s = 0; s < numSamples; ++s) {
            float inLevel = std::max(std::abs(scL[s]), std::abs(scR[s]));
            
            // Peak Detection
            float alpha = (inLevel > m_env) ? alphaA : alphaR;
            m_env = alpha * m_env + (1.0f - alpha) * inLevel;

            // Gain Reduction Logic
            float reduction = 1.0f;
            if (m_env > m_threshold) {
                float dbOver = 20.0f * std::log10(m_env / m_threshold);
                float dbReduced = dbOver * (1.0f / m_ratio - 1.0f);
                reduction = std::pow(10.0f, dbReduced / 20.0f);
            }

            // Smoothing for gain (Avoid Zipper noise)
            m_currentGain = 0.95f * m_currentGain + 0.05f * reduction;

            l[s] *= m_currentGain;
            r[s] *= m_currentGain;
        }
    }

    void reset() noexcept override {
        m_env = 0.0f;
        m_currentGain = 1.0f;
    }

    // Parameters
    void setThreshold(float t) { m_threshold = t; }
    void setRatio(float r) { m_ratio = r; }
    void setAttack(float ms) { m_attack = ms; }
    void setRelease(float ms) { m_release = ms; }

private:
    double m_sampleRate = 44100.0;
    float m_threshold, m_ratio, m_attack, m_release;
    float m_env = 0.0f;
    float m_currentGain = 1.0f;
};

} // namespace Aura::DSP::Effects
