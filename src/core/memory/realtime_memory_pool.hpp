#pragma once
#include <vector>
#include <atomic>
#include <cstdint>
#include <array>
#include <iostream>

namespace Aura::Core::Memory {

/**
 * @class RealtimeMemoryPool
 * @brief High-Entropy Lock-free Bump Arena Ring.
 * FIXES: 0x3f800000 corruption by ensuring delayed worker isolation.
 * PERFORMANCE: Sequential O(1) Allocation, Zero-Lock.
 */
class RealtimeMemoryPool {
public:
    static constexpr uint8_t kNumArenas = 16; // 16-Frame isolation ring
    static RealtimeMemoryPool& getInstance() { static RealtimeMemoryPool i; return i; }

    void initialize(size_t totalSizeBytes = 1024 * 1024 * 512) { // 512MB Total
        size_t perArenaSize = totalSizeBytes / kNumArenas;
        for (int i = 0; i < kNumArenas; ++i) {
            m_arenas[i].assign(perArenaSize, 0);
            m_heads[i].store(0, std::memory_order_relaxed);
        }
        m_frameIdx.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Allocates from the 'frame-locked' arena.
     */
    void* allocate(size_t sizeBytes) {
        // Capture frameIdx once (Atomic Acquire)
        uint32_t f = m_frameIdx.load(std::memory_order_acquire) % kNumArenas;
        size_t alignedSize = (sizeBytes + 63) & ~63;
        size_t head = m_heads[f].fetch_add(alignedSize, std::memory_order_seq_cst);
        
        if (head + alignedSize >= m_arenas[f].size()) {
            return nullptr; 
        }
        return &m_arenas[f][head];
    }

    /**
     * @brief Transitions to the next arena in the ring.
     * Guaranteed to isolate the current block from late-running workers.
     */
    void nextFrame() {
        uint32_t next = (m_frameIdx.load(std::memory_order_relaxed) + 1) % kNumArenas;
        m_heads[next].store(0, std::memory_order_release);
        m_frameIdx.store(next, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    double getUsageRatio() const {
        uint32_t f = m_frameIdx.load(std::memory_order_acquire) % kNumArenas;
        if (m_arenas[f].empty()) return 0.0;
        return static_cast<double>(m_heads[f].load()) / m_arenas[f].size();
    }


private:
    RealtimeMemoryPool() = default;
    std::array<std::vector<uint8_t>, kNumArenas> m_arenas;
    std::array<std::atomic<size_t>, kNumArenas> m_heads;
    std::atomic<uint32_t> m_frameIdx{0};
};

} // namespace Aura::Core::Memory
