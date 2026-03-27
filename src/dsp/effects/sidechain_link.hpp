#pragma once

#include <vector>
#include <atomic>
#include "../../core/audio_buffer.hpp"

namespace Aura::DSP::Effects {

/**
 * @brief SidechainLink: Enables inter-track dynamic routing.
 * Addresses the "missing sidechain infrastructure" from the review.
 */
class SidechainLink {
public:
    struct Envelope {
        float level = 0.0f;
    };

    /**
     * @brief Updates the sidechain level from a source track.
     */
    void updateFromSource(const Core::AudioBuffer& buffer) {
        float rms = 0.0f;
        const float* l = buffer.getWritePointer(0);
        for (uint32_t s = 0; s < buffer.getNumSamples(); ++s) {
            rms += l[s] * l[s];
        }
        m_level.store(std::sqrt(rms / buffer.getNumSamples()));
    }

    float getLevel() const { return m_level.load(); }

private:
    std::atomic<float> m_level{0.0f};
};

} // namespace Aura::DSP::Effects
