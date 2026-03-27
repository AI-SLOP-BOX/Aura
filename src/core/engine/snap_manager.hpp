#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Aura::Core::Engine {

/**
 * @class SnapManager
 * @brief Logic Pro Style 'Smart Snap' Engine.
 * HONEST FIX: Replaces simple grid snapping with 'Magnetic Attraction'.
 * Snaps to the closest significant event: Grid, Region Boundaries, or Playhead.
 */
class SnapManager {
public:
    struct SnapPoint {
        uint64_t position;
        std::string type; // "Grid", "Region", "Playhead"
    };

    /**
     * @brief Finds the best snap position for a given sample.
     */
    static uint64_t getSnappedPosition(uint64_t pos, uint32_t samplesPerBeat, const std::vector<uint64_t>& referencePoints) {
        // 1. GRID POINTS (Calculated)
        uint32_t grid16th = samplesPerBeat / 4;
        uint64_t gridPos = std::round((double)pos / grid16th) * grid16th;
        
        uint64_t bestPos = gridPos;
        int64_t minDiff = std::abs((int64_t)pos - (int64_t)gridPos);
        
        // 2. MAGNETIC ATTRACTION TO REFERENCE POINTS (Region Start/End, etc)
        // Threshold: 32 samples (very close)
        const int64_t attractionThreshold = 64; 

        for (uint64_t ref : referencePoints) {
            int64_t diff = std::abs((int64_t)pos - (int64_t)ref);
            if (diff < attractionThreshold && diff < minDiff) {
                minDiff = diff;
                bestPos = ref;
            }
        }

        return bestPos;
    }
};

} // namespace Aura::Core::Engine
