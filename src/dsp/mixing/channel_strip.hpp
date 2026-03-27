#pragma once

#include <atomic>
#include <cmath>
#include "panning_law.hpp"
#include "../../core/audio_buffer.hpp"
#include "../../core/atomic_parameter.hpp"

namespace Aura::DSP::Mixing {

/**
 * @brief ChannelStrip: Standard mixing unit for mono/stereo sources.
 * HONEST FIX: Replaced raw atomics with AtomicParameter to eliminate zipper noise.
 */
class ChannelStrip {
public:
    ChannelStrip() {
        m_gain.setSmoothingTime(20.0);
        m_pan.setSmoothingTime(20.0);
    }

    void setSampleRate(double sr) {
        m_gain.setSampleRate(sr);
        m_pan.setSampleRate(sr);
    }

    void setGain(float gain) { m_gain.setTarget(gain); }
    void setPan(float pan) { m_pan.setTarget(pan); } // -1.0 (L) to 1.0 (R)
    void setMute(bool mute) { m_mute.store(mute); }
    void setSolo(bool solo) { m_solo.store(solo); }

    /**
     * @brief Processes a stereo block with constant power panning and smoothing.
     */
    void process(Core::AudioBuffer& buffer) {
        if (m_mute.load()) {
            buffer.clear();
            return;
        }

        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        uint32_t numSamples = buffer.getNumSamples();

        for (uint32_t s = 0; s < numSamples; ++s) {
            float currentGain = m_gain.getNextValue();
            float currentPan = m_pan.getNextValue();
            
            float gainL, gainR;
            PanningLaw::calculate(currentPan, gainL, gainR);

            l[s] *= (currentGain * gainL);
            r[s] *= (currentGain * gainR);
        }
    }

private:
    Core::AtomicParameter m_gain{1.0f};
    Core::AtomicParameter m_pan{0.0f};
    std::atomic<bool> m_mute{false};
    std::atomic<bool> m_solo{false};
};

} // namespace Aura::DSP::Mixing
