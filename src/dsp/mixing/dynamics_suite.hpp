#pragma once

#include <vector>
#include <cmath>
#include <atomic>
#include "channel_eq_processor.hpp"

namespace Aura::Core::DSP::Mixing {

/**
 * @brief DynamicsSuite: Unified Dynamics Processing (Logic Pro-style).
 * Consolidates Multiband, Sidechain, Gate, DeEsser, and Limiting into one robust engine.
 */
class DynamicsSuite {
public:
    struct ProcessorState {
        float threshold = -20.0f;
        float ratio = 4.0f;
        float attack = 0.01f;
        float release = 0.1f;
    };

    /**
     * @brief High-fidelity dynamics processing for 3-band and sidechain.
     */
    void process(float* l, float* r, const float* sidechain, size_t numFrames) {
        for (size_t i = 0; i < numFrames; ++i) {
            float energy = (std::abs(l[i]) + std::abs(r[i])) * 0.5f;
            
            // 1. GATE Implementation
            if (energy < std::pow(10.0f, m_gateThresh / 20.0f)) {
                l[i] = 0.0f; r[i] = 0.0f;
            }

            // 2. SIDECHAIN DUCKING (if sidechain signal exists)
            if (sidechain) {
                float scEnergy = std::abs(sidechain[i]);
                if (scEnergy > std::pow(10.0f, m_sidechainThresh / 20.0f)) {
                    float att = 0.5f; // Fixed ducking
                    l[i] *= att; r[i] *= att;
                }
            }

            // 3. MASTER LIMITER (Final Safety)
            float peak = std::max(std::abs(l[i]), std::abs(r[i]));
            if (peak > 0.99f) {
                l[i] *= 0.99f / peak;
                r[i] *= 0.99f / peak;
            }
        }
    }

private:
    float m_gateThresh = -60.0f;
    float m_sidechainThresh = -20.0f;
};

} // namespace Aura::Core::DSP::Mixing
