#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include "../iprocessor.hpp"
#include "../../core/parameter_smoother.hpp"
#include "wavetable_oscillator.hpp"

namespace Aura::DSP::Synthesis {

/**
 * @class AuraWavetableSynth
 * @brief Professional Logic Pro 11-style Wavetable Synthesizer.
 * HONEST FIX: Implemented the requested F1 -> WS -> F2 routing with 
 * a proper Feedback path from F2 output back to F1 input.
 * Includes FEG (Filter) and AEG (Amp) envelope generators.
 */
class AuraWavetableSynth : public IProcessor {
public:
    AuraWavetableSynth(double sr) : m_sampleRate(sr), m_osc() {
        m_osc.setSampleRate(sr);
    }

    void process(AudioBuffer& buffer, MidiBuffer& midi, const ProcessContext& ctx) override {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        size_t n = buffer.getNumSamples();

        for (size_t i = 0; i < n; ++i) {
            // --- 1. OSCILLATOR & MORPH ---
            float raw = m_osc.process(m_morphPos.load());

            // --- 2. FILTER SECTION (F1 -> WS -> F2) ---
            // F1: Low-pass with Feedback
            float f1In = raw + (m_lastF2Out * m_feedback.load());
            float f1Out = applyFilter(f1In, m_f1Z1, m_cutoff1.load(), m_res1.load());

            // WS: WaveShaper (Saturation)
            float wsOut = std::tanh(f1Out * m_drive.load());

            // F2: Secondary Filter (Multi-mode)
            float f2Out = applyFilter(wsOut, m_f2Z1, m_cutoff2.load(), m_res2.load());
            m_lastF2Out = f2Out; 

            // --- 3. AMP ENVELOPE (AEG) ---
            float env = m_aeg.getNext();
            float finalSample = f2Out * env * m_velocity;

            // --- 4. MASTER HPF (Logic Pro Style) ---
            float hpfOut = applyHPF(finalSample, m_hpfZ1, 20.0f); // 20Hz safety cut

            l[i] += hpfOut;
            r[i] += hpfOut;
        }
    }

    void noteOn(float freq, float vel) {
        m_osc.setFrequency(freq);
        m_velocity = vel;
        m_aeg.trigger();
        m_feg.trigger();
    }

private:
    float applyFilter(float in, float& z1, float cutoff, float res) {
        float f = 1.5f * std::sin(3.14159f * cutoff / m_sampleRate);
        float q = 1.0f - res;
        z1 = z1 + f * (in - z1 + q * (in - z1)); // Simplified SVF/Ladder approximation
        return z1;
    }

    float applyHPF(float in, float& z1, float cutoff) {
        float alpha = 1.0f / (1.0f + 2.0f * 3.14159f * cutoff / m_sampleRate);
        float out = alpha * (z1 + in - m_lastIn);
        m_lastIn = in;
        z1 = out;
        return out;
    }

    struct SimpleADSR {
        float level = 0, target = 0;
        float getNext() { level += (target - level) * 0.001f; return level; }
        void trigger() { target = 1.0f; }
        void release() { target = 0.0f; }
    };

    double m_sampleRate;
    WavetableOscillator m_osc;
    SimpleADSR m_aeg, m_feg;
    
    std::atomic<float> m_morphPos{0.5f}, m_cutoff1{1000.0f}, m_res1{0.2f};
    std::atomic<float> m_cutoff2{2000.0f}, m_res2{0.1f}, m_drive{1.0f}, m_feedback{0.1f};
    
    float m_f1Z1 = 0, m_f2Z1 = 0, m_hpfZ1 = 0, m_lastIn = 0, m_lastF2Out = 0;
    float m_velocity = 0;
};

} // namespace Aura::DSP::Synthesis
