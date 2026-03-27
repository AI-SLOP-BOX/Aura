#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../mixing/state_variable_filter.hpp"
#include "analog_saturator.hpp"

namespace Aura::DSP::Effects {

/**
 * @brief MultibandExciter: Top-tier frequency-specific saturation.
 * Standard tool for 'Mastering' and 'Drum' processing.
 */
class MultibandExciter : public IProcessor {
public:
    MultibandExciter(double sr = 44100.0) : m_sampleRate(sr), m_satLow(sr), m_satMid(sr), m_satHigh(sr) {
        setupCrossover(200.0f, 3000.0f);
    }

    void setupCrossover(float lowCut, float highCut) {
        m_lowPass.setParameters(lowCut, 0.707f, 0); // Low band LP
        m_midLowPass.setParameters(highCut, 0.707f, 0); // Mid band LP
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        for (uint32_t i = 0; i < numSamples; ++i) {
            float inL = l[i], inR = r[i];

            // 1. FREQUENCY SPLITTING (LR-4 style approximation)
            float lowL = m_lowPass.processSampleLP(inL);
            float lowR = m_lowPass.processSampleLP(inR);

            float midHighL = inL - lowL;
            float midHighR = inR - lowR;

            float midL = m_midLowPass.processSampleLP(midHighL);
            float midR = m_midLowPass.processSampleLP(midHighR);

            float highL = midHighL - midL;
            float highR = midHighR - midR;

            // 2. APPLY SATURATION PER BAND (Meat)
            lowL = m_satLow.processSingle(lowL, 0.1f, 0.2f, AnalogSaturator::Model::Tube);
            lowR = m_satLow.processSingle(lowR, 0.1f, 0.2f, AnalogSaturator::Model::Tube);

            midL = m_satMid.processSingle(midL, 0.2f, 0.4f, AnalogSaturator::Model::FET);
            midR = m_satMid.processSingle(midR, 0.2f, 0.4f, AnalogSaturator::Model::FET);

            highL = m_satHigh.processSingle(highL, 0.4f, 0.6f, AnalogSaturator::Model::Tape);
            highR = m_satHigh.processSingle(highR, 0.4f, 0.6f, AnalogSaturator::Model::Tape);

            // 3. RECOMBINE (Zero-phase sum)
            l[i] = lowL + midL + highL;
            r[i] = lowR + midR + highR;
        }
    }

    void setSampleRate(double sr) override { m_sampleRate = sr; }
    uint32_t getLatency() const override { return 0; }

private:
    double m_sampleRate;
    Mixing::StateVariableFilter m_lowPass, m_midLowPass;
    AnalogSaturator m_satLow, m_satMid, m_satHigh;
};

} // namespace Aura::DSP::Effects
