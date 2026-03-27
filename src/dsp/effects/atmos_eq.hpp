#pragma once

#include <vector>
#include <array>
#include "../iprocessor.hpp"
#include "../mixing/state_variable_filter.hpp"

namespace Aura::DSP::Effects {

/**
 * @brief AtmosEQ: Professional 12-Channel Immersive Equalizer.
 * Standard for Atmos Mastering and Immersive Tonal Balance (7.1.4 Layout).
 */
class AtmosEQ : public IProcessor {
public:
    struct Band {
        float freq;
        float gain;
        float q;
        Mixing::StateVariableFilter::Mode mode;
    };

    AtmosEQ(double sr = 44100.0) : m_sampleRate(sr) {
        // Init 12 instances of filters (one set per channel)
        for (auto& ch : m_filters) {
            for (auto& f : ch) f.setSampleRate(sr);
        }
    }

    /**
     * @brief PROCESS IMMERSIVE: Applies identical EQ to up to 12 channels simultaneously.
     */
    void processImmersive(std::vector<float*>& buffers, uint32_t numSamples, const std::vector<Band>& bands) {
        for (uint32_t bIdx = 0; bIdx < bands.size(); ++bIdx) {
            const auto& b = bands[bIdx];
            for (uint32_t ch = 0; ch < std::min(static_cast<uint32_t>(buffers.size()), 12u); ++ch) {
                m_filters[ch][bIdx].setParameters(b.freq, b.q, b.gain);
                
                for (uint32_t i = 0; i < numSamples; ++i) {
                    buffers[ch][i] = m_filters[ch][bIdx].processSample(buffers[ch][i], b.mode);
                }
            }
        }
    }

    void process(float* l, float* r, uint32_t numSamples) override {
        // (Stereo fall-back logic)
    }

    void setSampleRate(double sr) override { m_sampleRate = sr; }
    uint32_t getLatency() const override { return 0; }

private:
    double m_sampleRate;
    // 12 Channels x 8 Bands (Typical)
    std::array<std::array<Mixing::StateVariableFilter, 8>, 12> m_filters;
};

} // namespace Aura::DSP::Effects
