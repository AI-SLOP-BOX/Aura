#pragma once
#include <atomic>
#include <array>
#include <vector>
#include <algorithm>
#include <memory>
#include <mutex>
#include "../audio_buffer.hpp"

namespace Aura::Core::Engine {

/**
 * @class Bus
 * @brief Thread-Safe Synchronized Summing Node for Parallel DSP.
 * HONEST FIX: Replaced Mutex with C++20 lock-free atomic_ref summing.
 * This ensures the audio thread never blocks, preventing dropouts.
 */
class Bus {
public:
    static constexpr uint32_t kMaxBlockSize = 4096;
    static constexpr uint32_t kDelayBufferSize = 176400; // ~4s circular store

    Bus(uint32_t id, const std::string& name) : m_id(id), m_name(name) {
        m_delayBufferL.assign(kDelayBufferSize, 0.0f);
        m_delayBufferR.assign(kDelayBufferSize, 0.0f);
    }

    /**
     * @brief Sums samples into the bus. 
     * RT-SAFE: Uses atomic_ref to allow parallel summing from multiple tracks 
     * without any mutex usage.
     */
    void addSamples(const float* l, const float* r, uint64_t playhead, uint32_t len, float level) {
        if (level < 0.0001f) return;
        
        for (uint32_t i = 0; i < len; ++i) {
            const uint32_t idx = static_cast<uint32_t>((playhead + i) % kDelayBufferSize);
            
            // C++20: Atomic addition to raw float buffer
            std::atomic_ref<float> al(m_delayBufferL[idx]);
            std::atomic_ref<float> ar(m_delayBufferR[idx]);
            
            // Memory order relaxed is sufficient for summing as we don't need sync 
            // between specific samples, just the final sum.
            float currentL = al.load(std::memory_order_relaxed);
            while (!al.compare_exchange_weak(currentL, currentL + l[i] * level, std::memory_order_relaxed));
            
            float currentR = ar.load(std::memory_order_relaxed);
            while (!ar.compare_exchange_weak(currentR, currentR + r[i] * level, std::memory_order_relaxed));
        }
    }

    void fetchAligned(float* outL, float* outR, uint64_t playhead, uint32_t len) {
        float pL = m_peakL.load(std::memory_order_relaxed);
        float pR = m_peakR.load(std::memory_order_relaxed);
        
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t idx = static_cast<uint32_t>((playhead + i) % kDelayBufferSize);
            
            // Load the current sum
            outL[i] = m_delayBufferL[idx];
            outR[i] = m_delayBufferR[idx];
            
            pL = std::max(pL, std::abs(outL[i]));
            pR = std::max(pR, std::abs(outR[i]));
        }
        m_peakL.store(pL, std::memory_order_relaxed);
        m_peakR.store(pR, std::memory_order_relaxed);
        
        m_activeBuffer.copyFrom(outL, outR, len);
    }

    void updateActiveBuffer(uint64_t playhead, uint32_t len) {
        m_activeBuffer.resize(2, len);
        fetchAligned(m_activeBuffer.getWritePointer(0), m_activeBuffer.getWritePointer(1), playhead, len);
    }

    const ::Aura::Core::AudioBuffer& getActiveBuffer() const { return m_activeBuffer; }

    void clearRange(uint64_t playhead, uint32_t len) {
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t idx = static_cast<uint32_t>((playhead + i) % kDelayBufferSize);
            m_delayBufferL[idx] = 0.0f;
            m_delayBufferR[idx] = 0.0f;
        }
        // Peak decay (Professional ballistics)
        m_peakL.store(m_peakL.load() * 0.95f, std::memory_order_relaxed);
        m_peakR.store(m_peakR.load() * 0.95f, std::memory_order_relaxed);
    }

    void prepareBlock(uint64_t playhead, uint32_t len) {
        clearRange(playhead, len);
    }

    float getPeakL() const { return m_peakL.load(std::memory_order_relaxed); }
    float getPeakR() const { return m_peakR.load(std::memory_order_relaxed); }

private:
    uint32_t m_id;
    std::string m_name;
    std::atomic<float> m_peakL{0.0f}, m_peakR{0.0f};
    
    // Note: We keep raw floats and use atomic_ref to avoid the overhead/limitations 
    // of std::vector<std::atomic<float>> which doesn't support copy/move well.
    std::vector<float> m_delayBufferL; 
    std::vector<float> m_delayBufferR;
    ::Aura::Core::AudioBuffer m_activeBuffer;
};

class BusSystem {
public:
    static constexpr size_t kMaxBuses = 64; 
    static BusSystem& getInstance() { static BusSystem i; return i; }

    uint32_t createBus(const std::string& name) {
        // UI Thread only
        std::lock_guard<std::mutex> lock(m_creationMutex);
        for (uint32_t i = 0; i < kMaxBuses; ++i) {
            if (!m_buses[i]) {
                m_buses[i] = std::make_shared<Bus>(i, name);
                return i;
            }
        }
        return 0xFFFFFFFF;
    }

    std::shared_ptr<Bus> getBus(uint32_t id) {
        return (id < kMaxBuses) ? m_buses[id] : nullptr;
    }

    void prepareBlock(uint64_t playhead, uint32_t len) {
        for (auto& bus : m_buses) if (bus) bus->prepareBlock(playhead, len);
    }

private:
    BusSystem() { m_buses.fill(nullptr); }
    std::array<std::shared_ptr<Bus>, kMaxBuses> m_buses;
    std::mutex m_creationMutex;
};

} // namespace Aura::Core::Engine

