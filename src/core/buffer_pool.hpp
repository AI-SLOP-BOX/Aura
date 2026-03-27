#pragma once
#include <array>
#include <atomic>
#include <vector>
#include "concurrency/lock_free.hpp"

namespace Aura::Core {

/**
 * @brief BufferPool: Truly lock-free, zero-allocation buffer recycler.
 * Essential for real-time safety during complex DSP graph processing.
 */
class BufferPool {
public:
    static constexpr size_t kMaxBuffers = 64;
    static constexpr size_t kSamplesPerBuffer = 8192;

    BufferPool() {
        for (size_t i = 0; i < kMaxBuffers; ++i) {
            m_buffers[i].assign(kSamplesPerBuffer, 0.0f);
            m_availableIndices.push(static_cast<uint32_t>(i));
        }
    }

    /**
     * @brief PooledBuffer: RAII handle for a buffer that returns to the pool automatically.
     */
    struct PooledBuffer {
        float* data = nullptr;
        uint32_t index = 0xFFFFFFFF;
        BufferPool* owner = nullptr;

        ~PooledBuffer() {
            if (owner && index != 0xFFFFFFFF) owner->m_availableIndices.push(index);
        }
        
        // Disable copy
        PooledBuffer(const PooledBuffer&) = delete;
        PooledBuffer& operator=(const PooledBuffer&) = delete;
        // Enable move
        PooledBuffer(PooledBuffer&& other) noexcept 
            : data(other.data), index(other.index), owner(other.owner) {
            other.owner = nullptr;
        }
    };

    /**
     * @brief Acquires a buffer from the pool without any locks.
     */
    PooledBuffer acquire() {
        auto idx = m_availableIndices.pop();
        if (idx) {
            return { m_buffers[*idx].data(), *idx, this };
        }
        return { nullptr, 0xFFFFFFFF, nullptr };
    }

private:
    std::array<std::vector<float>, kMaxBuffers> m_buffers;
    Concurrency::SPSCQueue<uint32_t, kMaxBuffers> m_availableIndices;
};

} // namespace Aura::Core
