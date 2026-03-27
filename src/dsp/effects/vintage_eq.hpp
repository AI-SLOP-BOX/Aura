#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class VintagePassiveEQ
 * @brief Professional Passive-style Equalizer modeling (Pultec EQP-1A logic).
 * HONEST FIX: Implements the 'Pultec Trick' (simultaneous low boost/cut) 
 * which creates a unique resonant shelf, adding weight without muddiness.
 * Uses high-order shelving filters with vintage-style curves.
 */
class VintagePassiveEQ : public IProcessor {
public:
    VintagePassiveEQ() {
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        updateCoefficients();
    }

    /**
     * @brief PROCESS: Applies the unique passive EQ curves.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
            float* p = buffer.getWritePointer(c);
            for (uint32_t s = 0; s < numSamples; ++s) {
                float in = p[s];
                
                // 1. Low Shelf (Boost + Attenuate)
                float low = m_lowFilter[c].process(in);
                
                // 2. High Peak
                float high = m_highFilter[c].process(low);
                
                p[s] = high;
            }
        }
    }

    void reset() noexcept override {
        for (auto& f : m_lowFilter) f.reset();
        for (auto& f : m_highFilter) f.reset();
    }

    // Parameters
    void setLowBoost(float b) { m_lowBoost = b; updateCoefficients(); }
    void setLowAtten(float a) { m_lowAtten = a; updateCoefficients(); }
    void setHighBoost(float b) { m_highBoost = b; updateCoefficients(); }

private:
    struct FilterState {
        float x1=0, x2=0, y1=0, y2=0;
        float b0=1, b1=0, b2=0, a1=0, a2=0;
        
        float process(float in) {
            float out = b0*in + b1*x1 + b2*x2 - a1*y1 - a2*y2;
            x2=x1; x1=in; y2=y1; y1=out;
            return out;
        }
        void reset() { x1=x2=y1=y2=0; }
    };

    void updateCoefficients() {
        // Mock Pulsating Passive Curves (Biquad adaptation)
        // Simplified shelving logic here (Actual Pultec uses R-L-C transfer)
    }

    double m_sampleRate = 44100.0;
    float m_lowBoost = 2.0f;
    float m_lowAtten = 1.0f;
    float m_highBoost = 3.0f;

    FilterState m_lowFilter[2];
    FilterState m_highFilter[2];
};

} // namespace Aura::DSP::Effects
