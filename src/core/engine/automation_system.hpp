#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <algorithm>
#include <mutex>

namespace Aura::Core::Engine {

/**
 * @class AutomationSystem
 * @brief Professional Sample-Accurate Automation Engine.
 * HONEST FIX: Replaced block-jumping with true sample-accurate linear interpolation.
 * Supports up to 4096 tracks with zero-allocation real-time performance.
 */
class AutomationSystem {
public:
    static constexpr size_t kMaxTracks = 4096;
    static constexpr size_t kMaxParamsPerTrack = 64;

    static AutomationSystem& getInstance() { static AutomationSystem i; return i; }

    struct ParameterBlock {
        std::array<float, kMaxParamsPerTrack> currentValues;
        std::array<float, kMaxParamsPerTrack> targetValues;
        std::array<float, kMaxParamsPerTrack> stepSizes;
    };

    /**
     * @brief Pre-calculates step sizes for the upcoming block.
     * Call this at the start of each audio block.
     */
    void prepareBlock(uint32_t numSamples) {
        float invSamples = 1.0f / static_cast<float>(std::max(1u, numSamples));
        
        uint32_t active = m_activeTracks.load(std::memory_order_acquire);
        for (uint32_t t = 0; t < active; ++t) {
            auto& block = m_blocks[t];
            for (uint32_t p = 0; p < kMaxParamsPerTrack; ++p) {
                float target = m_targets[t][p].load(std::memory_order_relaxed);
                block.targetValues[p] = target;
                block.stepSizes[p] = (target - block.currentValues[p]) * invSamples;
            }
        }
    }

    /**
     * @brief Increments a parameter by one sample step.
     */
    inline float nextValue(uint32_t trackId, uint32_t paramId) {
        auto& block = m_blocks[trackId];
        block.currentValues[paramId] += block.stepSizes[paramId];
        return block.currentValues[paramId];
    }

    /**
     * @brief Fills a buffer with sample-accurate automation values.
     */
    void getRamp(uint32_t trackId, uint32_t paramId, float* buffer, uint32_t numSamples) {
        auto& block = m_blocks[trackId];
        float val = block.currentValues[paramId];
        float step = block.stepSizes[paramId];
        
        for (uint32_t i = 0; i < numSamples; ++i) {
            val += step;
            buffer[i] = val;
        }
        block.currentValues[paramId] = val;
    }

    void setTarget(uint32_t trackId, uint32_t paramId, float value) {
        if (trackId < kMaxTracks && paramId < kMaxParamsPerTrack) {
            m_targets[trackId][paramId].store(value, std::memory_order_release);
        }
    }

    uint32_t addTrack() {
        uint32_t id = m_activeTracks.fetch_add(1, std::memory_order_acq_rel);
        return (id < kMaxTracks) ? id : 0;
    }

private:
    AutomationSystem() {
        m_blocks.resize(kMaxTracks);
        m_targets.resize(kMaxTracks);
        m_activeTracks.store(0);
    }
    
    std::vector<ParameterBlock> m_blocks; // Audio thread internal state
    std::vector<std::array<std::atomic<float>, kMaxParamsPerTrack>> m_targets; // Shared bridge
    std::atomic<uint32_t> m_activeTracks;
};

} // namespace Aura::Core::Engine
