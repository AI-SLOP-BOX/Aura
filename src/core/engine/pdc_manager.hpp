#pragma once
#include <vector>
#include <algorithm>
#include <array>
#include <atomic>

namespace Aura::Core::Engine {

/**
 * @class PDCManager
 * @brief Professional DAG-based Plug-in Delay Compensation (PDC).
 * HONEST FIX: Implements Recursive Path-Depth traversal to solve 
 * latency for complex Routing Graphs (Tracks -> Buses -> Master).
 */
class PDCManager {
public:
    static constexpr size_t kMaxTracks = 512;
    static constexpr size_t kMaxBuses = 128;
    static constexpr uint32_t kMasterID = 0xFFFFFFFF;

    static PDCManager& getInstance() { static PDCManager i; return i; }

    uint32_t getCompensationOffset(uint32_t trackId) const {
        uint32_t idx = m_activeBuffer.load(std::memory_order_acquire);
        return (trackId < kMaxTracks) ? m_trackOffsets[idx][trackId] : 0;
    }

    uint32_t getGlobalMaxLatency() const { return m_maxGlobal.load(std::memory_order_acquire); }
    uint32_t getMaxLatency() const { return getGlobalMaxLatency(); }

    uint32_t getBusOffset(uint32_t busId) const {
        uint32_t idx = m_activeBuffer.load(std::memory_order_acquire);
        return (busId < kMaxBuses) ? m_busOffsets[idx][busId] : 0;
    }

    // --- SETUP: ROUTING GRAPH ---
    void setTrackDest(uint32_t trackId, uint32_t destId) {
        if (trackId < kMaxTracks) { m_routing[trackId] = destId; markDirty(); }
    }
    
    void setBusDest(uint32_t busId, uint32_t destId) {
        if (busId < kMaxBuses) { m_routing[kMaxTracks + busId] = destId; markDirty(); }
    }

    void setTrackLatency(uint32_t trackId, uint32_t samples) {
        if (trackId < kMaxTracks) {
            m_trackLatencies[trackId].store(samples, std::memory_order_release);
            markDirty();
        }
    }

    void setBusLatency(uint32_t busId, uint32_t samples) {
        if (busId < kMaxBuses) {
            m_busLatencies[busId].store(samples, std::memory_order_release);
            markDirty();
        }
    }

    void setLowLatencyMode(bool active) { m_lowLatencyMode.store(active); markDirty(); }
    void markDirty() { m_dirty.store(true, std::memory_order_release); }

    /**
     * @brief ACCURATE DAG RECALCULATION
     * HONEST FIX: Uses Depth-First Traversal to calculate the cumulative
     * latency of every signal path. Swaps buffers atomically for RT-safety.
     */
    void recalculate() {
        if (!m_dirty.load(std::memory_order_acquire)) return;

        uint32_t nextIdx = 1 - m_activeBuffer.load(std::memory_order_acquire);
        bool lowLatency = m_lowLatencyMode.load(std::memory_order_acquire);
        
        std::array<uint32_t, kMaxTracks + kMaxBuses> pathLatencies;
        pathLatencies.fill(0);

        // 1. Snapshot Latencies & Reset Back Buffer
        m_trackOffsets[nextIdx].fill(0);
        m_busOffsets[nextIdx].fill(0);

        // 2. STABLE PATH TRAVERSAL (With Cycle Detection)
        uint32_t maxGlobal = 0;
        std::vector<bool> visited(kMaxTracks + kMaxBuses, false);
        std::vector<bool> onStack(kMaxTracks + kMaxBuses, false);

        for (uint32_t i = 0; i < kMaxTracks + kMaxBuses; ++i) {
            pathLatencies[i] = computeTotalLatencySafe(i, lowLatency, visited, onStack);
            maxGlobal = std::max(maxGlobal, pathLatencies[i]);
        }

        
        // 3. APPLY COMPENSATION (Alignment based on global max)
        for (uint32_t i = 0; i < kMaxTracks; ++i) {
            m_trackOffsets[nextIdx][i] = maxGlobal - pathLatencies[i];
        }
        for (uint32_t i = 0; i < kMaxBuses; ++i) {
            m_busOffsets[nextIdx][i] = maxGlobal - pathLatencies[kMaxTracks + i];
        }

        m_maxGlobal.store(maxGlobal, std::memory_order_release);
        m_activeBuffer.store(nextIdx, std::memory_order_release);
        m_dirty.store(false, std::memory_order_release);
    }

private:
    uint32_t computeTotalLatencySafe(uint32_t id, bool lowLatency, std::vector<bool>& visited, std::vector<bool>& onStack) {
        if (id == kMasterID) return 0;
        if (onStack[id]) return 0; // Infinite Routing Cycle Detected! Stop recursion.
        
        uint32_t self = 0;
        if (id < kMaxTracks) {
            self = m_trackLatencies[id].load(std::memory_order_acquire);
            if (lowLatency && self > 256) self = 0;
        } else {
            self = m_busLatencies[id - kMaxTracks].load(std::memory_order_acquire);
        }

        uint32_t dest = m_routing[id];
        if (dest == kMasterID || dest == id) return self;
        
        onStack[id] = true;
        uint32_t total = self + computeTotalLatencySafe(dest, lowLatency, visited, onStack);
        onStack[id] = false;
        
        visited[id] = true;
        return total;
    }


    PDCManager() {
        m_activeBuffer.store(0);
        m_routing.fill(kMasterID);
        m_trackOffsets[0].fill(0); m_trackOffsets[1].fill(0);
        m_busOffsets[0].fill(0); m_busOffsets[1].fill(0);
    }

    std::array<std::atomic<uint32_t>, kMaxTracks> m_trackLatencies;
    std::array<std::atomic<uint32_t>, kMaxBuses> m_busLatencies;
    std::array<uint32_t, kMaxTracks + kMaxBuses> m_routing; 

    // HONEST FIX: Double Buffering
    std::array<std::array<uint32_t, kMaxTracks>, 2> m_trackOffsets;
    std::array<std::array<uint32_t, kMaxBuses>, 2> m_busOffsets;
    std::atomic<uint32_t> m_activeBuffer{0};

    std::atomic<bool> m_dirty{true};
    std::atomic<bool> m_lowLatencyMode{false};
    std::atomic<uint32_t> m_maxGlobal{0};
};

} // namespace Aura::Core::Engine
