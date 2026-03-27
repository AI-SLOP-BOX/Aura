#pragma once
#include <vector>
#include <map>
#include <mutex>
#include <future>
#include <algorithm>
#include <string>

namespace Aura::UI::Main {

/**
 * @brief WaveformThumbnailer: Multi-threaded GPU-ready peak generator.
 */
class WaveformThumbnailer {
public:
    static WaveformThumbnailer& getInstance() {
        static WaveformThumbnailer instance;
        return instance;
    }

    struct PeakData {
        // Interleaved [min, max] for GPU SSBO compatibility
        std::vector<float> peakPairs; 
    };

    /**
     * @brief Generates peaks for a given audio buffer in a background thread.
     */
    void generateAsync(const std::string& assetPath, const float* data, size_t numSamples, size_t resolution = 128) {
        std::async(std::launch::async, [this, assetPath, data, numSamples, resolution]() {
            PeakData peaks;
            peaks.peakPairs.reserve((numSamples / resolution) * 2);
            
            for (size_t i = 0; i < numSamples; i += resolution) {
                float mn = 1.0f, mx = -1.0f;
                size_t end = std::min(i + resolution, numSamples);
                
                // SIMD-friendly loop for professional high-speed peaking
                for (size_t j = i; j < end; ++j) {
                    float val = data[j];
                    if (val < mn) mn = val;
                    if (val > mx) mx = val;
                }
                
                peaks.peakPairs.push_back(mn);
                peaks.peakPairs.push_back(mx);
            }
            
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cache[assetPath] = std::move(peaks);
        });
    }

    const PeakData* getPeaks(const std::string& assetPath) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(assetPath);
        return (it != m_cache.end()) ? &it->second : nullptr;
    }

private:
    WaveformThumbnailer() = default;
    
    std::map<std::string, PeakData> m_cache;
    std::mutex m_cacheMutex;
};

} // namespace Aura::UI::Main
