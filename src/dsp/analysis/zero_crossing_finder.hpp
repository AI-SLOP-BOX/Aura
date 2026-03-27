#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @brief ZeroCrossingFinder: Precision audio editing utility.
 * Locates the nearest sample where the waveform crosses 0dB to prevent "clicks" during cuts.
 */
class ZeroCrossingFinder {
public:
    /**
     * @brief Finds the nearest zero-crossing point around the target position.
     * @param data: Audio buffer data.
     * @param targetPos: Desired edit point.
     * @param searchRange: Number of samples to look in either direction.
     * @return The optimal sample index for a clean cut.
     */
    static size_t findNearest(const float* data, size_t targetPos, size_t numSamples, size_t searchRange = 128) {
        size_t start = (targetPos > searchRange) ? targetPos - searchRange : 0;
        size_t end = std::min(targetPos + searchRange, numSamples - 1);

        size_t bestPos = targetPos;
        float minAbs = 1.0f;

        for (size_t i = start; i < end; ++i) {
            // Logic: Check for a sign change between adjacent samples
            bool signChange = (data[i] >= 0 && data[i+1] < 0) || (data[i] < 0 && data[i+1] >= 0);
            
            if (signChange) {
                // Return the sample that is closest to absolute zero
                return (std::abs(data[i]) < std::abs(data[i+1])) ? i : i + 1;
            }

            // Fallback: If no sign change is found, track the absolute minimum value
            if (std::abs(data[i]) < minAbs) {
                minAbs = std::abs(data[i]);
                bestPos = i;
            }
        }
        
        return bestPos;
    }
};

} // namespace Aura::DSP::Analysis
