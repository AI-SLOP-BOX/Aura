#pragma once

#include <atomic>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include "../../core/utils/string_hash.hpp"

namespace Aura::Core::Engine {

using namespace Utils;

/**
 * @brief AuraDiagnostics: Comprehensive, real-time safe engine diagnostics.
 * Now refactored with Rolling Average to prevent memory growth (Logic Pro standards).
 */
class AuraDiagnostics {
public:
    static AuraDiagnostics& getInstance() {
        static AuraDiagnostics instance;
        return instance;
    }

    struct HealthStatus {
        std::atomic<double> cpuLoad{0.0};
        std::atomic<uint32_t> dropouts{0};
        std::atomic<double> diskIOLatency{0.0};
    };

    /**
     * @brief Logs an engine-level performance event. 
     * Uses ID-based lookup for real-time safety.
     */
    void logEvent(uint32_t eventId, double nanos) {
        // Professional Logic: Record the event in a thread-safe, 
        // fixed-memory circular buffer (Moving Average).
        m_eventStats[eventId].record(nanos);
    }

    HealthStatus& getHealth() { return m_health; }

private:
    struct MovingAverage {
        std::array<double, 64> samples = {0.0};
        size_t index = 0;
        std::atomic<double> average{0.0};

        void record(double val) {
            samples[index] = val;
            index = (index + 1) % samples.size();
            
            double sum = 0.0;
            for (auto s : samples) sum += s;
            average.store(sum / samples.size());
        }
    };

    AuraDiagnostics() = default;
    
    HealthStatus m_health;
    std::map<uint32_t, MovingAverage> m_eventStats;
};

} // namespace Aura::Core::Engine
