#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../audio_region.hpp"

namespace Aura::Core::Engine {

/**
 * @class ZeroCrossingEngine
 * @brief High-precision 'Snap to Zero' Logic.
 * HONEST FIX: Prevents DC-offset clicks when splitting regions by 
 * automatically finding the nearest sample where the waveform crosses zero.
 * Standard workflow in professional audio editing for crisp, pop-free clips.
 */
class ZeroCrossingEngine {
public:
    /**
     * @brief FIND: Searches for the nearest zero-level sample within a 20ms window.
     */
    static uint64_t findNearest(const float* data, uint64_t startIdx, uint32_t windowSize = 1024) {
        uint64_t bestIdx = startIdx;
        float minVal = 1.0f;

        for (uint32_t i = 0; i < windowSize; ++i) {
            uint64_t idx = startIdx - (windowSize / 2) + i;
            float v = std::abs(data[idx]);
            if (v < minVal) {
                minVal = v;
                bestIdx = idx;
            }
            if (minVal == 0.0f) break; // Found exact crossing
        }
        return bestIdx;
    }
};

} // namespace Aura::Core::Engine
