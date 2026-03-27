#pragma once

#include <cmath>
#include <algorithm>
#include <vector>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class DynamicCompressor
 * @brief Professional Mastering-Grade Dynamic Range Processor.
 * HONEST FIX: Added Sidechain Support, Soft Knee, and RMS Detection.
 */
class DynamicCompressor : public IProcessor {
public:
    DynamicCompressor() {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        // --- HONEST FIX: PRE-ALLOCATE RT-SAFE ---
        // Avoids memory allocation during process() even if lookahead changes.
        m_delayL.assign(kMaxLookahead, 0.0f);
        m_delayR.assign(kMaxLookahead, 0.0f);
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer&, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        
        // 2. SIDECHAIN KEY SELECTION

        const float* scL = (context.sidechainBuffer) ? context.sidechainBuffer->getReadPointer(0) : l;
        const float* scR = (context.sidechainBuffer) ? context.sidechainBuffer->getReadPointer(1) : r;

        float totalMakeupDB = m_makeupDB + (m_autoGain ? calculateAutoMakeup() : 0.0f);

        for (uint32_t s = 0; s < numSamples; ++s) {
            // DETECTOR (on current signal)
            float detectorLevel = std::max(std::abs(scL[s]), std::abs(scR[s]));
            if (m_useRMS) {
                m_rmsSum = 0.999f * m_rmsSum + 0.001f * (detectorLevel * detectorLevel);
                detectorLevel = std::sqrt(m_rmsSum);
            }
            
            float targetAlpha = (detectorLevel > m_envelope) ? m_attackAlpha : m_releaseAlpha;
            m_envelope = targetAlpha * m_envelope + (1.0f - targetAlpha) * detectorLevel;

            // GAIN COMPUTATION
            // --- HONEST FIX: OPTIMIZED LOG10 (Approximation) ---
            // Replaces expensive std::log10 with a vectorized-friendly fast-log.
            float envDB = 0.0f;
            if (m_envelope > 1e-7f) {
                union { float f; uint32_t i; } vx = { m_envelope };
                float log2 = (float)vx.i * 1.1920928955078125e-7f - 126.94269504f;
                envDB = log2 * 3.0102999566f; // Log2 to Log10 conversion
            } else {
                envDB = -140.0f;
            }

            float delta = envDB - m_thresholdDB;
            float gainDB = 0.0f;
            if (delta > 0.5f * m_kneeDB) gainDB = delta * (1.0f / m_ratio - 1.0f);
            else if (delta > -0.5f * m_kneeDB) {
                float x = delta + m_kneeDB * 0.5f;
                gainDB = (1.0f / m_ratio - 1.0f) * x * x / (2.0f * m_kneeDB);
            }


            float targetGr = std::pow(10.0f, (gainDB + totalMakeupDB) / 20.0f);
            m_currentGr = 0.99f * m_currentGr + 0.01f * targetGr;

            // LOOK-AHEAD: Store current sample and emit delayed one
            m_delayL[m_writeIdx] = l[s];
            m_delayR[m_writeIdx] = r[s];
            
            uint32_t readIdx = (m_writeIdx + kMaxLookahead - m_lookaheadSamples) % kMaxLookahead;
            l[s] = m_delayL[readIdx] * m_currentGr;
            r[s] = m_delayR[readIdx] * m_currentGr;
            
            m_writeIdx = (m_writeIdx + 1) % kMaxLookahead;
        }
    }

    uint32_t getLatencySamples() const noexcept override { return m_lookaheadSamples; }

    void reset() noexcept override {
        m_envelope = 0.0f; m_currentGr = 1.0f; m_rmsSum = 0.0f;
        std::fill(m_delayL.begin(), m_delayL.end(), 0.0f);
        std::fill(m_delayR.begin(), m_delayR.end(), 0.0f);
    }

    // Parameters
    void setLookahead(float ms) { m_lookaheadSamples = static_cast<uint32_t>(m_sampleRate * ms * 0.001f); }
    void setThreshold(float db) { m_thresholdDB = db; }
    void setRatio(float r) { m_ratio = r; }
    void setAttack(float ms) { m_attackAlpha = std::exp(-1.0f / (m_sampleRate * ms * 0.001f)); }
    void setRelease(float ms) { m_releaseAlpha = std::exp(-1.0f / (m_sampleRate * ms * 0.001f)); }
    void setMakeup(float db) { m_makeupDB = db; }
    void setAutoGain(bool enable) { m_autoGain = enable; }
    void setKnee(float db) { m_kneeDB = std::max(0.0f, db); }
    void setUseRMS(bool enable) { m_useRMS = enable; }

private:
    float calculateAutoMakeup() const { return -(m_thresholdDB * (1.0f - 1.0f / m_ratio)) * 0.5f; }

    static constexpr uint32_t kMaxLookahead = 4096;
    double m_sampleRate = 44100.0;
    float m_thresholdDB = -20.0f, m_ratio = 4.0f, m_makeupDB = 0.0f, m_kneeDB = 6.0f;
    bool m_autoGain = true, m_useRMS = false;
    float m_rmsSum = 0.0f, m_attackAlpha = 0.9f, m_releaseAlpha = 0.999f, m_envelope = 0.0f, m_currentGr = 1.0f;
    
    std::vector<float> m_delayL, m_delayR;
    uint32_t m_writeIdx = 0, m_lookaheadSamples = 0;
};

} // namespace Aura::DSP::Effects
