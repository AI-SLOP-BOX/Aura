#pragma once

#include <chrono>
#include <atomic>

namespace Aura::Core {

/**
 * @brief CPUMeter: Monitors the audio callback performance in real-time.
 * Addresses the "lack of performance monitoring" from the review.
 */
class CPUMeter {
public:
    CPUMeter(double sampleRate) : m_sampleRate(sampleRate) {}

    /**
     * @brief Starts the timer for the current audio block.
     */
    void startBlock() {
        m_start = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Ends the timer and calculates the CPU load percentage.
     */
    void endBlock(uint32_t numSamples) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
        
        double maxTime = (static_cast<double>(numSamples) / m_sampleRate) * 1e9; // Max nanoseconds
        m_load.store(static_cast<float>(duration / maxTime) * 100.0f);
    }

    float getLoad() const { return m_load.load(); }

private:
    double m_sampleRate;
    std::chrono::high_resolution_clock::time_point m_start;
    std::atomic<float> m_load{0.0f};
};

} // namespace Aura::Core
