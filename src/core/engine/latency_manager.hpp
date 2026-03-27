#pragma once

#include <vector>
#include <map>
#include <atomic>
#include <algorithm>

namespace Aura::Core::Engine {

/**
 * @brief LatencyManager: Professional Plugin Delay Compensation (PDC) engine.
 * Ensures all audio tracks remain phase-aligned by compensating for look-ahead processing.
 */
class LatencyManager {
public:
    static LatencyManager& getInstance() {
        static LatencyManager instance;
        return instance;
    }

    /**
     * @brief Registers the latency (in samples) for a specific track.
     */
    void registerLatency(uint32_t trackId, uint32_t latencySamples) {
        m_trackLatencies[trackId] = latencySamples;
        calculateTotalLatency();
    }

    /**
     * @brief Returns the maximum latency found in the entire project.
     */
    uint32_t getMaxLatency() const { return m_maxLatency.load(); }

    /**
     * @brief Returns how many samples a specific track should be DELAYED 
     * to match the project's maximum latency.
     */
    uint32_t getCompensationFor(uint32_t trackId) const {
        auto it = m_trackLatencies.find(trackId);
        uint32_t trackLat = (it != m_trackLatencies.end()) ? it->second : 0;
        return m_maxLatency.load() - trackLat;
    }

private:
    LatencyManager() = default;

    void calculateTotalLatency() {
        uint32_t currentMax = 0;
        for (const auto& pair : m_trackLatencies) {
            currentMax = std::max(currentMax, pair.second);
        }
        m_maxLatency.store(currentMax);
    }

    std::map<uint32_t, uint32_t> m_trackLatencies;
    std::atomic<uint32_t> m_maxLatency{0};
};

} // namespace Aura::Core::Engine
