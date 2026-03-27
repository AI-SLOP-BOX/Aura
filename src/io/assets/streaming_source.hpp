#pragma once

#include <vector>
#include <atomic>
#include <fstream>
#include <mutex>

#include "../../core/audio_region.hpp"

namespace Aura::Core::Assets {

class StreamingSource : public ::Aura::Core::IAudioSource {
public:
    float getSample(uint32_t channel, uint64_t sampleIdx) const override { 
        // Note: Real implementation would seek or use cached buffers.
        // For now return dummy or 0.0f
        return 0.0f; 
    }
    uint64_t getNumSamples() const override { return getTotalSamples(); }
    uint32_t getNumChannels() const override { return 2; }
public:
    static constexpr size_t kBufferSize = 65536; // 64k buffer

    StreamingSource(const std::string& filePath) : m_filePath(filePath) {
        m_file.open(filePath, std::ios::binary);
        if (m_file.is_open()) {
            m_file.seekg(0, std::ios::end);
            m_totalSamples = m_file.tellg() / sizeof(float);
            m_file.seekg(0, std::ios::beg);
        }
        m_buffers[0].resize(kBufferSize);
        m_buffers[1].resize(kBufferSize);
        refill();
    }

    /**
     * @brief Checks if the background buffer needs refilling from disk.
     */
    bool needsRefill() const {
        return m_needsRefill.load(std::memory_order_acquire);
    }

    /**
     * @brief Performed by the IO Worker thread. Reads the next block into the inactive buffer.
     */
    void refill() {
        if (!m_file.is_open()) return;

        size_t inactiveIdx = 1 - m_activeIdx.load();
        m_file.read(reinterpret_cast<char*>(m_buffers[inactiveIdx].data()), kBufferSize * sizeof(float));
        
        m_needsRefill.store(false, std::memory_order_release);
    }

    /**
     * @brief Consumed by the Audio Thread.
     */
    float getNextSample() {
        size_t activeIdx = m_activeIdx.load(std::memory_order_acquire);
        float sample = m_buffers[activeIdx][m_readPos];

        m_readPos++;
        if (m_readPos >= kBufferSize) {
            m_activeIdx.store(1 - activeIdx, std::memory_order_release);
            m_readPos = 0;
            m_needsRefill.store(true, std::memory_order_release);
        }
        return sample;
    }

    uint64_t getTotalSamples() const { return m_totalSamples; }
    std::string getFilePath() const override { return m_filePath; }

private:
    uint64_t m_totalSamples = 0;
    std::string m_filePath;
    std::ifstream m_file;
    
    std::vector<float> m_buffers[2];
    std::atomic<size_t> m_activeIdx{0};
    uint64_t m_readPos = 0;
    
    std::atomic<bool> m_needsRefill{false};
};

} // namespace Aura::Core::Assets
