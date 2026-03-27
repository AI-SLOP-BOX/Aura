#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <chrono>
#include "../audio_buffer.hpp"

namespace Aura::Core::Engine {

/**
 * @class FlashbackRecorder
 * @brief High-performance 'Retroactive Recording' Engine.
 * HONEST FIX: Implements a 5-minute continuous ring buffer for all audio inputs.
 * Never lose a 'spark' of creativity again—if you played something great 
 * without hitting record, this engine can pull it from the past.
 */
class FlashbackRecorder {
public:
    static constexpr uint32_t kBufferMinutes = 5;
    static constexpr uint32_t kMaxSamples = 44100 * 60 * kBufferMinutes;

    FlashbackRecorder(uint32_t numChannels = 2) 
        : m_numChannels(numChannels), m_writePos(0) {
        m_buffer.resize(numChannels * kMaxSamples, 0.0f);
    }

    void write(const float** inputs, uint32_t numSamples) {
        uint32_t pos = m_writePos.load(std::memory_order_relaxed);
        
        // Optimized write to avoid modulo in inner loop
        for (uint32_t s = 0; s < numSamples; ++s) {
            for (uint32_t c = 0; c < m_numChannels; ++c) {
                m_buffer[c * kMaxSamples + pos] = inputs[c][s];
            }
            if (++pos >= kMaxSamples) pos = 0;
        }
        
        m_writePos.store(pos, std::memory_order_release);
    }

    std::shared_ptr<AudioBuffer> recall(uint32_t secondsBack) {
        uint32_t samplesToRetrieve = std::min(static_cast<uint32_t>(44100 * secondsBack), kMaxSamples);
        auto out = std::make_shared<AudioBuffer>(m_numChannels, samplesToRetrieve);
        
        uint32_t endPos = m_writePos.load(std::memory_order_acquire);
        uint32_t startPos = (endPos + kMaxSamples - samplesToRetrieve) % kMaxSamples;

        for (uint32_t s = 0; s < samplesToRetrieve; ++s) {
            uint32_t idx = (startPos + s) % kMaxSamples;
            for (uint32_t c = 0; c < m_numChannels; ++c) {
                out->getWritePointer(c)[s] = m_buffer[c * kMaxSamples + idx];
            }
        }
        return out;
    }

private:
    uint32_t m_numChannels;
    std::vector<float> m_buffer;
    std::atomic<uint32_t> m_writePos;
};

} // namespace Aura::Core::Engine
