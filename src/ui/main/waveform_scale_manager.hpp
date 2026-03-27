#pragma once

#include <atomic>

namespace Aura::UI::Main {

/**
 * @brief WaveformScaleManager: Manages visual gain for waveform rendering.
 * Iconic Logic Pro feature that allows "Waveform Zoom" for quiet recordings.
 */
class WaveformScaleManager {
public:
    static WaveformScaleManager& getInstance() {
        static WaveformScaleManager instance;
        return instance;
    }

    /**
     * @brief Sets the global visual multiplier for waveform drawing.
     */
    void setVisualGain(float gain) { m_visualGain.store(gain); }
    float getVisualGain() const { return m_visualGain.load(); }

    /**
     * @brief Scales a raw sample for UI plotting.
     */
    float scaleSample(float raw) const {
        return raw * m_visualGain.load();
    }

private:
    WaveformScaleManager() : m_visualGain(1.0f) {}

    std::atomic<float> m_visualGain;
};

} // namespace Aura::UI::Main
