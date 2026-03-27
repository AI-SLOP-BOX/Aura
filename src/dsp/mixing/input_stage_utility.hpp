#pragma once

#include <atomic>

namespace Aura::Core::DSP::Mixing {

/**
 * @brief InputStageUtility: Pre-fader gain and phase management.
 * Iconic Logic Pro feature for channel head-amp simulation and drum-phase alignment.
 */
class InputStageUtility {
public:
    void setGainDB(float db) { m_gainLinear.store(std::pow(10.0f, db / 20.0f)); }
    void setPhaseInvert(bool invert) { m_isPhaseInverted.store(invert); }

    /**
     * @brief Normalizes input samples before they hit the plugin chain.
     */
    void process(float* l, float* r, size_t numFrames) {
        float gain = m_gainLinear.load();
        float multiplier = m_isPhaseInverted.load() ? -1.0f : 1.0f;
        float totalFactor = gain * multiplier;

        for (size_t i = 0; i < numFrames; ++i) {
            l[i] *= totalFactor;
            r[i] *= totalFactor;
        }
    }

private:
    std::atomic<float> m_gainLinear{1.0f};
    std::atomic<bool> m_isPhaseInverted{false};
};

} // namespace Aura::Core::DSP::Mixing
