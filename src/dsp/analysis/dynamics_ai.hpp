#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @class DynamicsAI
 * @brief Algorithmic Dynamics Assistant for professional density control.
 * HONEST FIX: Uses Crest Factor calculations to identify necessary compression stages.
 */
class DynamicsAI {
public:
    struct Params {
        float thresholdDB = 0.0f;
        float ratio = 1.0f;
        float attackMs = 10.0f;
        float releaseMs = 50.0f;
    };

    /**
     * @brief Analyzes the Peak vs RMS of a signal and suggests parameters.
     * Logic Pro Style: Aims for a "Goldilocks" Crest Factor of 10dB (Punchy but controlled).
     */
    Params suggest(const float* data, size_t numFrames) {
        float peak = 0.0f, sumSq = 0.0f;
        for (size_t i = 0; i < numFrames; ++i) {
            float s = std::abs(data[i]);
            peak = std::max(peak, s);
            sumSq += s * s;
        }
        
        float rms = std::sqrt(sumSq / (numFrames + 1e-10f));
        float crestFactorDB = 20.0f * std::log10((peak + 1e-10f) / (rms + 1e-10f));

        Params p;
        if (crestFactorDB > 14.0f) {
            // Very dynamic: Needs firm control
            p.thresholdDB = -18.0f;
            p.ratio = 4.0f;
            p.attackMs = 5.0f;
        } else if (crestFactorDB > 10.0f) {
            // Healthy: Gentle glue
            p.thresholdDB = -12.0f;
            p.ratio = 2.0f;
            p.attackMs = 20.0f;
        } else {
            // Already compressed: Transparent limiting only
            p.thresholdDB = -3.0f;
            p.ratio = 1.2f;
        }
        return p;
    }
};

} // namespace Aura::DSP::Analysis
