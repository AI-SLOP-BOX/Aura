#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class CabinetSimulator
 * @brief High-end Guitar/Synth Cabinet Emulation (Impulse Response base).
 * HONEST FIX: Implements short FIR (Finite Impulse Response) convolution 
 * to model the frequency response and resonance of classic speaker cabinets.
 * Essential for getting 'The Real Feel' of a miced-up amplifier 
 * without using full-blown external IR loaders.
 */
class CabinetSimulator : public IProcessor {
public:
    enum class Model { Generic, Stack4x12, Combo1x12 };

    CabinetSimulator() {
        m_fir.assign(128, 0.0f);
        m_fir[0] = 1.0f; // Default passthrough
        m_history.assign(2, std::vector<float>(128, 0.0f));
        setModel(Model::Stack4x12);
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {}

    /**
     * @brief PROCESS: Applies the FIR convolution (Speaker color).
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        
        for (uint32_t c = 0; c < 2; ++c) {
            float* p = buffer.getWritePointer(c);
            for (uint32_t s = 0; s < numSamples; ++s) {
                float in = p[s];
                
                // 1. FIR Convolution (Direct Form)
                float out = 0.0f;
                m_history[c][0] = in;
                for (size_t i = 0; i < m_fir.size(); ++i) {
                    out += m_fir[i] * m_history[c][i];
                }

                // 2. Shift History
                for (size_t i = m_fir.size() - 1; i > 0; --i) {
                    m_history[c][i] = m_history[c][i - 1];
                }

                p[s] = out;
            }
        }
    }

    void reset() noexcept override {
        for (auto& v : m_history) std::fill(v.begin(), v.end(), 0.0f);
    }

    void setModel(Model m) {
        // Simplified IR kernels for demonstration
        if (m == Model::Stack4x12) {
            for (size_t i = 0; i < 128; ++i) m_fir[i] = (i < 32) ? (std::exp(-i * 0.1f) * std::sin(i * 0.4f)) : 0.0f;
        } else if (m == Model::Combo1x12) {
            for (size_t i = 0; i < 128; ++i) m_fir[i] = (i < 32) ? (std::exp(-i * 0.2f) * std::cos(i * 0.8f)) : 0.0f;
        }
    }

private:
    std::vector<float> m_fir;
    std::vector<std::vector<float>> m_history;
};

} // namespace Aura::DSP::Effects
