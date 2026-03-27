#pragma once

#include <vector>
#include <atomic>

namespace Aura::DSP::Effects {

/**
 * @brief DelayLine: Professional-grade sample-accurate delay.
 * Used for PDC (Plugin Delay Compensation) to align tracks in time.
 */
class DelayLine {
public:
    DelayLine(uint32_t maxDelaySamples) {
        m_buffer.resize(maxDelaySamples, 0.0f);
    }

    /**
     * @brief Processes a single sample through the delay and returns the delayed value.
     */
    float process(float sample, uint32_t delaySamples) {
        if (delaySamples == 0) return sample;

        uint32_t size = static_cast<uint32_t>(m_buffer.size());
        m_buffer[m_writeIdx] = sample;
        
        int readIdx = static_cast<int>(m_writeIdx) - static_cast<int>(delaySamples);
        while (readIdx < 0) readIdx += static_cast<int>(size);
        
        float delayed = m_buffer[static_cast<uint32_t>(readIdx) % size];
        m_writeIdx = (m_writeIdx + 1) % size;
        
        return delayed;
    }

    void reset() {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
        m_writeIdx = 0;
    }

private:
    std::vector<float> m_buffer;
    uint32_t m_writeIdx = 0;
};

} // namespace Aura::DSP::Effects
