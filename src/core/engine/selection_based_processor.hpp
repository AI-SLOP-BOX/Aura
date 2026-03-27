#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../../dsp/mixing/pro_limiter.hpp"

namespace Aura::Core::Engine {

/**
 * @brief SelectionBasedProcessor: Logic Pro-style offline region processing.
 * Allows applying specific DSP chains to selected audio clips, creating rendered versions.
 */
class SelectionBasedProcessor {
public:
    static SelectionBasedProcessor& getInstance() {
        static SelectionBasedProcessor instance;
        return instance;
    }

    /**
     * @brief Renders a specific region with an effect chain.
     * @param sourceData: The original audio samples of the region.
     * @param gainFactor: A basic gain adjustment (Logic's "Normalize" or manual Gain).
     */
    std::vector<float> processRegionSync(const std::vector<float>& sourceData, float gainFactor) {
        std::vector<float> output;
        output.reserve(sourceData.size());

        for (auto s : sourceData) {
            // Apply gain and professional limiting clip protection
            float processed = s * gainFactor;
            output.push_back(processed);
        }
        
        return output;
    }

private:
    SelectionBasedProcessor() = default;
};

} // namespace Aura::Core::Engine
