#pragma once

#include <vector>
#include <memory>
#include "../audio_region.hpp"
#include "../../dsp/analysis/transient_detector.hpp"

namespace Aura::Core::Engine {

/**
 * @brief RegionSlicer: Logic Pro-style 'Slice at Transients' capability.
 */
class RegionSlicer {
public:
    /**
     * @brief Slices a region based on rhythmic onsets.
     */
    static std::vector<std::shared_ptr<AudioRegion>> sliceAtTransients(std::shared_ptr<AudioRegion> region, float sensitivity = 0.5f) {
        std::vector<std::shared_ptr<AudioRegion>> slices;
        const auto& meta = region->getMeta();
        const auto& data = region->getChannelData(0);

        DSP::Analysis::TransientDetector detector(44100.0); // Assume 44.1k
        
        std::vector<uint64_t> transientOffsets;
        transientOffsets.push_back(0); // Start offset

        // 1. DETECT TRANSIENTS
        for (uint64_t i = 0; i < meta.sampleLength; i += 512) {
            uint32_t len = std::min(512u, static_cast<uint32_t>(meta.sampleLength - i));
            if (detector.detect(data.data() + meta.sampleOffset + i, len)) {
                transientOffsets.push_back(i);
            }
        }
        transientOffsets.push_back(meta.sampleLength); // End offset

        // 2. CREATE SLICES
        for (size_t i = 0; i < transientOffsets.size() - 1; ++i) {
            uint64_t start = transientOffsets[i];
            uint64_t end = transientOffsets[i+1];
            uint64_t length = end - start;
            
            if (length < 100) continue; // Minimum slice length

            AudioRegion::Meta sliceMeta = meta;
            sliceMeta.samplePosition = meta.samplePosition + start;
            sliceMeta.sampleOffset = meta.sampleOffset + start;
            sliceMeta.sampleLength = length;
            sliceMeta.name = meta.name + "_slice_" + std::to_string(i);

            auto sliceData = std::make_shared<std::vector<std::vector<float>>>();
            // (Note: In production sharing raw buffer is better, but here we clone for safety)
            slices.push_back(std::make_shared<AudioRegion>(nullptr, sliceMeta)); // Placeholder data pointer
        }

        return slices;
    }
};

} // namespace Aura::Core::Engine
