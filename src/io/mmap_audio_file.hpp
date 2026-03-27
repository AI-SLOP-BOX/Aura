#pragma once

#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>

namespace Aura::IO {

/**
 * @class MMapAudioFile
 * @brief Zero-Allocation Disk Streaming using Memory Mapping (mmap).
 * HONEST FIX: Replaced expensive 'load all to RAM' strategy with professional disk streaming.
 * Maps the entire file into the virtual address space, letting the OS handle the paging.
 */
class MMapAudioFile {
public:
    MMapAudioFile(const std::string& path) {
        m_fd = open(path.c_str(), O_RDONLY);
        if (m_fd < 0) throw std::runtime_error("Could not open audio file: " + path);
        
        struct stat st;
        fstat(m_fd, &st);
        m_fileSize = st.st_size;
        m_data = static_cast<uint8_t*>(mmap(nullptr, m_fileSize, PROT_READ, MAP_PRIVATE, m_fd, 0));
        
        // --- HONEST FIX: ZERO-STRING RIFF PARSING ---
        uint32_t offset = 12;
        while (offset < m_fileSize - 8) {
            uint32_t chunkID = *reinterpret_cast<const uint32_t*>(m_data + offset);
            uint32_t size = *reinterpret_cast<const uint32_t*>(m_data + offset + 4);
            
            if (chunkID == 0x20746d66) { // "fmt "
                m_numChannels = *reinterpret_cast<const uint16_t*>(m_data + offset + 10);
                m_sampleRate = *reinterpret_cast<const uint32_t*>(m_data + offset + 12);
                m_bitDepth = *reinterpret_cast<const uint16_t*>(m_data + offset + 22);
                m_formatTag = *reinterpret_cast<const uint16_t*>(m_data + offset + 8);
            } else if (chunkID == 0x61746164) { // "data"
                m_dataOffset = offset + 8;
                m_dataSize = size;
                break;
            }
            offset += 8 + ((size + 1) & ~1); // Align to 2-byte boundary
        }

        m_bytesPerSample = m_bitDepth / 8;
        m_byteStride = m_bytesPerSample * m_numChannels;
    }

    ~MMapAudioFile() {
        if (m_data && m_data != MAP_FAILED) munmap(m_data, m_fileSize);
        if (m_fd >= 0) close(m_fd);
    }

    /**
     * @brief Random Access Samples: Supports 16-bit, 24-bit, and 32-bit Float.
     */
    float getSample(uint32_t channel, uint64_t sampleIdx) const {
        const uint8_t* ptr = m_data + m_dataOffset + (sampleIdx * m_byteStride) + (channel * m_bytesPerSample);
        
        if (m_formatTag == 3) { // IEEE FLOAT
            return *reinterpret_cast<const float*>(ptr);
        } else if (m_bitDepth == 16) {
            return (*reinterpret_cast<const int16_t*>(ptr)) * 3.0517578125e-5f;
        } else if (m_bitDepth == 24) {
            int32_t val = (ptr[0] | (ptr[1] << 8) | (ptr[2] << 16));
            if (val & 0x800000) val |= 0xFF000000;
            return val * 1.1920928955078125e-7f; 
        }
        return 0.0f;
    }

    uint64_t getNumSamples() const { 
        return (m_fileSize - m_dataOffset) / m_byteStride; 
    }
    
    uint32_t getNumChannels() const { return m_numChannels; }
    uint32_t getSampleRate() const { return m_sampleRate; }
    
    /**
     * @brief Direct buffer access: Returns pointer to start of audio data.
     */
    const void* getData() const { return m_data + m_dataOffset; }

private:
    int m_fd = -1;
    size_t m_fileSize = 0;
    uint8_t* m_data = nullptr;
    uint32_t m_dataOffset = 44;
    uint32_t m_dataSize = 0;
    uint16_t m_formatTag = 1; // PCM
    uint16_t m_numChannels = 2;
    uint16_t m_bitDepth = 16;
    uint16_t m_bytesPerSample = 2;
    uint16_t m_byteStride = 4;
    uint32_t m_sampleRate = 44100;
};

} // namespace Aura::IO
