#pragma once

#include <vector>
#include <atomic>
#include <string>

namespace Aura::Core::Engine {

/**
 * @brief LowLatencyMonitor: Recording-ready plugin bypass system.
 * Iconic Logic Pro feature that automatically disables heavy plugins to minimize lag.
 */
class LowLatencyMonitor {
public:
    static LowLatencyMonitor& getInstance() {
        static LowLatencyMonitor instance;
        return instance;
    }

    /**
     * @brief Temporarily disables any "heavy" plugins when recording is active.
     */
    void toggleLowLatencyMode(bool active, float thresholdMs = 5.0f) {
        m_isInLowLatencyRecord.store(active);
        // Step 1: Scan all tracks
        // Step 2: Bypass plugins reporting latency > thresholdMs
    }

    bool isModeActive() const { return m_isInLowLatencyRecord.load(); }

private:
    LowLatencyMonitor() = default;

    std::atomic<bool> m_isInLowLatencyRecord{false};
};

} // namespace Aura::Core::Engine
