#include <cmath>
#include <algorithm>
#include <array>
#include "../../core/audio_buffer.hpp"
#include "../../core/midi_buffer.hpp"
#include "../iprocessor.hpp"
#include "../utils/pitch_shifter.hpp"

namespace Aura::DSP::Effects {

/**
 * @class VirtuosoPitch
 * @brief Real-time Intelligent Pitch Correction.
 */
class VirtuosoPitch : public IProcessor {
public:
    VirtuosoPitch(double sr = 44100.0) : m_sampleRate(sr) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        uint32_t numSamples = buffer.getNumSamples();
        
        // --- 1. PITCH DETECTION (Zero-Crossing Autocorrelation) ---
        for (uint32_t s = 0; s < numSamples; ++s) {
            float in = buffer.getReadPointer(0)[s];
            if ((in > 0.0f && m_lastIn <= 0.0f) || (in < 0.0f && m_lastIn >= 0.0f)) {
                float period = static_cast<float>(m_writeCount - m_lastCross);
                m_detectedPitch = m_sampleRate / (period * 2.0f);
                m_lastCross = m_writeCount;
            }
            m_lastIn = in;
            m_writeCount++;
        }

        // --- 2. INTELLIGENT SNAP-TO-SCALE (Logic Pro Style) ---
        m_targetPitch = snapToScale(m_detectedPitch);
        float shiftRatio = m_targetPitch / (m_detectedPitch + 1e-6f);

        // --- 3. PITCH SHIFTING ---
        m_shifterL.process(buffer.getWritePointer(0), numSamples, shiftRatio, m_sampleRate);
        m_shifterR.process(buffer.getWritePointer(1), numSamples, shiftRatio, m_sampleRate);
    }

    void reset() noexcept override {
        m_writeCount = 0;
        m_lastCross = 0;
        m_shifterL.reset();
        m_shifterR.reset();
    }

private:
    float snapToScale(float freq) {
        // Find nearest semitone (A4 = 440)
        float semitones = 69.0f + 12.0f * std::log2(freq / 440.0f);
        float nearest = std::round(semitones);
        return 440.0f * std::pow(2.0f, (nearest - 69.0f) / 12.0f);
    }

    double m_sampleRate;
    Utils::PitchShifter m_shifterL, m_shifterR;
    uint64_t m_writeCount = 0, m_lastCross = 0;
    float m_lastIn = 0.0f;
    float m_detectedPitch = 440.0f, m_targetPitch = 440.0f;
};

} // namespace Aura::DSP::Effects
