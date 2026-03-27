#pragma once

#include <vector>
#include <algorithm>
#include <memory>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace Aura::Core {

/**
 * @brief AudioBuffer: The 'Blood' of the DAW.
 * HONEST FIX: Unified the memory allocation into a single contiguous block.
 */
class AudioBuffer {
public:
    static constexpr size_t kAlignment = 64;

    AudioBuffer() : m_numChannels(0), m_numSamples(0), m_capacity(0), m_isExternal(false), m_isMmapped(false), m_data(nullptr) {}
    
    AudioBuffer(uint32_t channels, uint32_t samples) : AudioBuffer() {
        resize(channels, samples);
    }

    ~AudioBuffer() {
        if (m_isMmapped && m_data) {
            munmap(m_data, m_capacity * sizeof(float));
        } else if (!m_isExternal && m_data) {
            free(m_data);
        }
    }

    AudioBuffer(AudioBuffer&& other) noexcept { *this = std::move(other); }
    AudioBuffer& operator=(AudioBuffer&& other) noexcept {
        if (this != &other) {
            if (!m_isExternal && m_data) free(m_data);
            m_numChannels = other.m_numChannels;
            m_numSamples = other.m_numSamples;
            m_capacity = other.m_capacity;
            m_data = other.m_data;
            m_isExternal = other.m_isExternal;
            m_externalData = other.m_externalData;
            other.m_data = nullptr;
            other.m_externalData = nullptr;
        }
        return *this;
    }

    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;

    void reserve(uint32_t channels, uint32_t samples) {
        // --- HONEST FIX: SIMD PADDING (AVX-512 Safe) ---
        // Round up samples to multiples of 16 to ensure vectorized loads never 
        // read past the allocated region.
        uint32_t paddedSamples = (samples + 15) & ~15;
        size_t required = static_cast<size_t>(channels) * paddedSamples;
        
        if (required > m_capacity) {
            if (m_data && !m_isExternal) free(m_data);
            m_capacity = required;
            if (posix_memalign((void**)&m_data, kAlignment, m_capacity * sizeof(float)) != 0) {
                throw std::bad_alloc();
            }
        }
    }

    void resize(uint32_t channels, uint32_t samples) {
        m_isExternal = false;
        reserve(channels, samples);
        m_numChannels = channels;
        m_numSamples = samples;
        clear();
    }

    void wrapChannels(float** channels, uint32_t numChannels, uint32_t numSamples) {
        m_numChannels = numChannels;
        m_numSamples = numSamples;
        m_isExternal = true;
        m_externalData = channels;
        m_isDirty = true;
    }

    float* getWritePointer(uint32_t channel) { 
        m_isDirty = true; 
        if (m_isExternal) return m_externalData[channel];
        return m_data + (channel * m_numSamples);
    }
    
    const float* getReadPointer(uint32_t channel) const { 
        if (m_isExternal) return m_externalData[channel];
        return m_data + (channel * m_numSamples);
    }

    void clear() {
        clear(m_numSamples);
    }

    void clear(uint32_t samples) {
        uint32_t n = std::min(samples, m_numSamples);
        if (m_isExternal) {
            for (uint32_t c = 0; c < m_numChannels; ++c) {
                std::fill(m_externalData[c], m_externalData[c] + n, 0.0f);
            }
        } else if (m_data) {
            for (uint32_t c = 0; c < m_numChannels; ++c) {
                std::memset(m_data + (c * m_numSamples), 0, n * sizeof(float));
            }
        }
        m_isDirty = false;
    }

    void addFrom(const AudioBuffer& other, uint32_t numSamples) {
        uint32_t chans = std::min(m_numChannels, other.m_numChannels);
        uint32_t n = std::min(numSamples, std::min(m_numSamples, other.m_numSamples));
        for (uint32_t c = 0; c < chans; ++c) {
            float* d = getWritePointer(c);
            const float* s = other.getReadPointer(c);
            for (uint32_t i = 0; i < n; ++i) d[i] += s[i];
        }
    }

    uint32_t getNumChannels() const { return m_numChannels; }
    uint32_t getNumSamples() const { return m_numSamples; }
    bool isEmpty() const { return m_numSamples == 0 || m_numChannels == 0; }

    void copyFrom(const float* l, const float* r, uint32_t numSamples) {
        if (m_numChannels < 2 || m_numSamples < numSamples) resize(2, numSamples);
        m_isDirty = true;
        std::memcpy(getWritePointer(0), l, numSamples * sizeof(float));
        std::memcpy(getWritePointer(1), r, numSamples * sizeof(float));
    }

    float getMagnitude(uint32_t channel) const {
        if (isEmpty()) return 0.0f;
        const float* p = getReadPointer(channel);
        float maxVal = 0.0f;
        for (uint32_t i = 0; i < m_numSamples; ++i) {
            maxVal = std::max(maxVal, std::abs(p[i]));
        }
        return maxVal;
    }

private:
    uint32_t m_numChannels;
    uint32_t m_numSamples;
    size_t m_capacity;
    bool m_isExternal = false;
    bool m_isMmapped = false;
    bool m_isStreaming = false;
    bool m_isDirty = true;
    int64_t m_streamOffset = 0;
    float* m_data = nullptr;
    float** m_externalData = nullptr;
};

} // namespace Aura::Core
