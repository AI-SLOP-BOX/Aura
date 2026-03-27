#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include "../../core/audio_region.hpp"

namespace Aura::Rendering {

/**
 * @class WaveformOverview
 * @brief Professional multi-resolution peak cache.
 * Fixed: Actually stores and returns peak data for GPU acceleration.
 */
class WaveformOverview {
public:
    struct Peak { float min = 0.0f; float max = 0.0f; };
    struct LOD { uint32_t ratio; std::vector<float> minData; std::vector<float> maxData; };

    WaveformOverview(std::shared_ptr<::Aura::Core::IAudioSource> source) 
        : m_source(source) {
        if (m_source) {
            generateAsync({64, 512, 4096}); 
        }
    }

    const float* getMinData() const { return m_lods.empty() ? nullptr : m_lods[0].minData.data(); }
    const float* getMaxData() const { return m_lods.empty() ? nullptr : m_lods[0].maxData.data(); }
    size_t getNumPeaks() const { return m_lods.empty() ? 0 : m_lods[0].minData.size(); }

private:
    void generateAsync(std::vector<uint32_t> ratios) {
        std::thread([this, ratios]() {
            for (auto ratio : ratios) {
                LOD lodLevel; lodLevel.ratio = ratio;
                uint64_t totalS = m_source->getNumSamples();
                uint32_t numPoints = (uint32_t)(totalS / ratio);
                if (numPoints == 0) continue;
                lodLevel.minData.resize(numPoints);
                lodLevel.maxData.resize(numPoints);
                
                for (uint32_t i = 0; i < numPoints; ++i) {
                    float minV = 0, maxV = 0;
                    for (uint32_t s = 0; s < ratio; ++s) {
                        float v = m_source->getSample(0, i * ratio + s);
                        minV = std::min(minV, v); maxV = std::max(maxV, v);
                    }
                    lodLevel.minData[i] = minV;
                    lodLevel.maxData[i] = maxV;
                }
                
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lods.push_back(std::move(lodLevel));
            }
        }).detach(); 
    }

private:
    std::shared_ptr<::Aura::Core::IAudioSource> m_source;
    std::vector<LOD> m_lods;
    std::mutex m_mutex;
};

} // namespace Aura::Rendering
