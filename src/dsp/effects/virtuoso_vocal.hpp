#include <cmath>
#include <algorithm>
#include <array>
#include "../../core/audio_buffer.hpp"
#include "../iprocessor.hpp"
#include "../utils/pitch_shifter.hpp"
#include "../utils/zdf_filter.hpp"

namespace Aura::DSP::Effects {

/**
 * @class VirtuosoVocal
 * @brief High-end Pitch & Formant Shifter (Vocal Transformer).
 */
class VirtuosoVocal : public IProcessor {
public:
    VirtuosoVocal(double sr = 44100.0) : m_sampleRate(sr) {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        m_formantFilterL.setSampleRate(sr);
        m_formantFilterR.setSampleRate(sr);
    }

    std::string getName() const override { return "Virtuoso Vocal"; }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        uint32_t numSamples = buffer.getNumSamples();
        float pitchRatio = std::pow(2.0f, m_pitchShiftSemi / 12.0f);
        
        // --- 1. Pitch Shifting ---
        m_shifterL.process(buffer.getWritePointer(0), numSamples, pitchRatio, m_sampleRate);
        m_shifterR.process(buffer.getWritePointer(1), numSamples, pitchRatio, m_sampleRate);

        // --- 2. Formant Shifting (Band-Pass Peak Shifting) ---
        float formantFreq = 800.0f * std::pow(2.0f, m_formantShift / 12.0f);
        m_formantFilterL.updateCoefficients(formantFreq, 1.5f);
        m_formantFilterR.updateCoefficients(formantFreq, 1.5f);

        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        for (uint32_t s = 0; s < numSamples; ++s) {
            float wetL = m_formantFilterL.process(l[s]);
            float wetR = m_formantFilterR.process(r[s]);
            l[s] = (l[s] * 0.4f + wetL * 0.6f); // Blend faked formant
            r[s] = (r[s] * 0.4f + wetR * 0.6f);
        }
    }

    void reset() noexcept override {
        m_shifterL.reset();
        m_shifterR.reset();
        m_formantFilterL.reset();
        m_formantFilterR.reset();
    }

    void setPitchShift(float semitones) { m_pitchShiftSemi = semitones; }
    void setFormantShift(float shift) { m_formantShift = shift; }

private:
    double m_sampleRate;
    Utils::PitchShifter m_shifterL, m_shifterR;
    Utils::ZDFFilter m_formantFilterL, m_formantFilterR;
    float m_pitchShiftSemi = 0.0f;
    float m_formantShift = 0.0f;
};

/**
 * @class DeEsser
 * @brief Professional Sibilance Reducer.
 */
class DeEsser : public IProcessor {
public:
    DeEsser() : m_threshold(0.2f) {}
    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_filterL.setSampleRate(sr);
        m_filterR.setSampleRate(sr);
    }
    std::string getName() const override { return "DeEsser"; }
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        m_filterL.updateCoefficients(6000.0f, 0.707f, Utils::ZDFFilter::Type::HighPass);
        m_filterR.updateCoefficients(6000.0f, 0.707f, Utils::ZDFFilter::Type::HighPass);

        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        for (uint32_t s = 0; s < buffer.getNumSamples(); ++s) {
            float sibL = std::abs(m_filterL.process(l[s]));
            float sibR = std::abs(m_filterR.process(r[s]));
            float maxSib = std::max(sibL, sibR);
            float reduction = (maxSib > m_threshold) ? (1.0f - (maxSib - m_threshold) * 0.8f) : 1.0f;
            reduction = std::clamp(reduction, 0.2f, 1.0f);
            l[s] *= reduction;
            r[s] *= reduction;
        }
    }
    void reset() noexcept override {}
private:
    Utils::ZDFFilter m_filterL, m_filterR;
    float m_threshold;
};

} // namespace Aura::DSP::Effects
