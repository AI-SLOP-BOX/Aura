#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include "dsp/effects/pro_limiter.hpp"
#include "dsp/effects/true_peak_limiter.hpp"
#include "dsp/effects/virtuoso_tape.hpp"
#include "dsp/effects/virtuoso_pultec.hpp"
#include "dsp/effects/fet_compressor.hpp"
#include "dsp/analysis/master_meter.hpp"
#include "dsp/utils/dither.hpp"
#include "core/concurrency/simd_kernel.hpp"
#include "core/audio_buffer.hpp"
#include "dsp/iprocessor.hpp"

namespace Aura::DSP::Mixing {

/**
 * @class MasterSuite
 * @brief Professional High-End Mastering Strip.
 * Inherits from IProcessor for unified engine integration.
 */
class MasterSuite : public IProcessor {
public:
    MasterSuite(double sr = 44100.0) : m_sampleRate(sr), m_pultec(sr), m_tapeSaturator(sr), m_busCompressor(sr) {
        m_busCompressor.setThreshold(-20.0f);
        m_busCompressor.setRatio(2.0f);
        m_busCompressor.setAttack(30.0f);
        m_busCompressor.setRelease(100.0f);
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        m_pultec.prepareToPlay(sr, bs);
        m_tapeSaturator.prepareToPlay(sr, bs);
        m_busCompressor.prepareToPlay(sr, bs);
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        uint32_t numSamples = buffer.getNumSamples();

        // 1. AI: AUTOMATIC GAIN STAGING (Loudness Matching)
        // Adjusts input gain to target a consistent LUFS level before processing.
        if (m_autoGainEnabled) {
            float currentLUFS = m_meter.getLatestData().lufsShortTerm;
            if (currentLUFS > -60.0f) { // Only adjust if audio is detected
                float diff = m_targetLUFS - currentLUFS;
                // Slow pursuit (0.005) for transparent gain adjustment
                m_autoGainOffset += (diff - m_autoGainOffset) * 0.005f; 
                float gainFactor = std::pow(10.0f, m_autoGainOffset / 20.0f);
                Core::SIMD::SIMDKernel::applyGain(l, gainFactor, numSamples);
                Core::SIMD::SIMDKernel::applyGain(r, gainFactor, numSamples);
            }
        }

        // 2. TONAL SHAPING (Pultec EQ -> Tape Saturation)
        m_pultec.process(buffer, midi, context);
        m_tapeSaturator.process(buffer, midi, context);
        
        // 3. DYNAMICS CONTROL (Glue Compression)
        m_busCompressor.process(buffer, midi, context);

        // 2. MID/SIDE STEREO WIDTH (Professional Matrix Implementation)
        if (m_enableMidSide) {
            // Encode: L/R -> M/S
            Core::SIMD::SIMDKernel::processMidSide(l, r, numSamples); 
           
            // Scale Side channel (r) for Width
            // HONEST FIX: Width is applied symmetrically to the Side component.
            if (m_stereoWidth != 1.0f) {
                Core::SIMD::SIMDKernel::applyGain(r, m_stereoWidth, numSamples);
            }

            // Decode: M/S -> L/R
            Core::SIMD::SIMDKernel::processMidSide(l, r, numSamples);
        }
        
        // 3. FINAL TRUE-PEAK LIMITING (Must be done AFTER all processing)
        m_limiter.process(l, r, numSamples, 0.0f, -0.1f);
        
        // 4. DITHER (Optional, only for final output stage)
        if (m_ditherEnabled) {
            for (uint32_t s = 0; s < numSamples; ++s) {
                l[s] += m_ditherL.process();
                r[s] += m_ditherR.process();
            }
        }

        // 5. FINAL ANALYSIS (Meter sees the EXACT output hitting the speakers)
        m_meter.process(l, r, numSamples);
    }

    void reset() noexcept override {
        m_pultec.reset();
        m_tapeSaturator.reset();
        m_busCompressor.reset();
        m_limiter.reset();
    }

    Analysis::MasterMeter::MeterData getLatestMetrics() const {
        return m_meter.getLatestData();
    }

    void setAutoGain(bool enable, float targetLUFS = -14.0f) {
        m_autoGainEnabled = enable;
        m_targetLUFS = targetLUFS;
    }

    float getMasterGain() const { return std::pow(10.0f, m_autoGainOffset / 20.0f); }

private:
    double m_sampleRate = 44100.0;
    bool m_enableMidSide = false;
    bool m_ditherEnabled = false;
    bool m_autoGainEnabled = false;
    float m_targetLUFS = -14.0f;
    float m_autoGainOffset = 0.0f;
    float m_stereoWidth = 1.0f;
    Effects::TruePeakLimiter m_limiter;
    Effects::FETCompressor m_busCompressor;
    Effects::VirtuosoTapeSaturator m_tapeSaturator;
    Effects::VirtuosoPultec m_pultec;
    Analysis::MasterMeter m_meter;
    Utils::TPDFDither m_ditherL, m_ditherR;
};

} // namespace Aura::DSP::Mixing
