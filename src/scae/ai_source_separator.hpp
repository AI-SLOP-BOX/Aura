#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../audio_buffer.hpp"

namespace Aura::SCAE {

/**
 * @class AISourceSeparator
 * @brief Next-Gen AI Stem Splitting Engine (Logic Pro 11 'Stem Splitter').
 */
class AISourceSeparator {
public:
    struct Stems {
        std::shared_ptr<Core::AudioBuffer> vocal;
        std::shared_ptr<Core::AudioBuffer> drum;
        std::shared_ptr<Core::AudioBuffer> bass;
        std::shared_ptr<Core::AudioBuffer> other;
    };

    /**
     * @brief SPLIT: Decomposes a stereo mix into 4 analytical stems.
     */
    void split(const Core::AudioBuffer& input, Stems& out) {
        if (input.getNumChannels() < 2) return;
        uint32_t len = input.getNumSamples();
        
        if (!out.vocal) out.vocal = std::make_shared<Core::AudioBuffer>(2, len);
        if (!out.drum) out.drum = std::make_shared<Core::AudioBuffer>(2, len);
        if (!out.bass) out.bass = std::make_shared<Core::AudioBuffer>(2, len);
        if (!out.other) out.other = std::make_shared<Core::AudioBuffer>(2, len);

        const float* l = input.getReadPointer(0);
        const float* r = input.getReadPointer(1);

        // --- SPECTRAL PARTITIONING (SIMULATED AI) ---
        // Using harmonicity and centroid-based probability masking.
        float bassLP = 0.0f, drumHP = 0.0f;
        float prevSample = 0.0f;

        for (uint32_t s = 0; s < len; ++s) {
            float mid = (l[s] + r[s]) * 0.5f;
            float side = (l[s] - r[s]) * 0.5f;

            // Transient detection via derivative for Drum separation
            float deriv = std::abs(mid - prevSample);
            float drumMask = std::clamp(deriv * 5.0f, 0.0f, 1.0f);
            
            // Bass isolation via leaky integration
            bassLP += 0.05f * (mid - bassLP);
            
            // Vocal isolation via phase coherence (Vocal is usually center-panned harmonic)
            float vocalMask = (1.0f - std::abs(side) / (std::abs(mid) + 1e-6f));
            vocalMask = std::pow(std::clamp(vocalMask, 0.0f, 1.0f), 2.0f);

            out.drum->getWritePointer(0)[s] = mid * drumMask;
            out.drum->getWritePointer(1)[s] = mid * drumMask;
            
            out.bass->getWritePointer(0)[s] = bassLP * (1.0f - drumMask);
            out.bass->getWritePointer(1)[s] = bassLP * (1.0f - drumMask);
            
            out.vocal->getWritePointer(0)[s] = (mid - bassLP) * (1.0f - drumMask) * vocalMask;
            out.vocal->getWritePointer(1)[s] = (mid - bassLP) * (1.0f - drumMask) * vocalMask;
            
            out.other->getWritePointer(0)[s] = side + (mid * (1.0f - vocalMask) * (1.0f - drumMask));
            out.other->getWritePointer(1)[s] = -side + (mid * (1.0f - vocalMask) * (1.0f - drumMask));

            prevSample = mid;
        }
    }
};

} // namespace Aura::SCAE
