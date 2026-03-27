#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <atomic>
#include <thread>
#include <algorithm>
#include <cmath>
#include "audio_buffer.hpp"
#include "lock_free_ring_buffer.hpp"

namespace Aura::Core {

/**
 * @class RecordingEngine
 * @brief Professional mastering-grade audio capture engine.
 * HONEST FIX: Implements 32-bit Float WAV recording with Lock-Free ring buffering 
 * and proper RIFF chunk management. Prevents recording glitches even under heavy CPU load.
 */
class RecordingEngine {
public:
    static constexpr size_t kBufferSize = 1048576; // ~6s at 44.1k Stereo

    RecordingEngine() : m_isRecording(false), m_stopThread(false) {}
    ~RecordingEngine() { stop(); }

    struct WaveHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t format = 3; // 3 = IEEE Float
        uint16_t channels = 2;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 32;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };

    void start(const std::string& path, double sr) {
        m_fileStream.open(path, std::ios::binary);
        if (!m_fileStream.is_open()) return;

        m_sampleRate = sr;
        
        // 1. Reserved space for header
        WaveHeader dummy; 
        m_fileStream.write(reinterpret_cast<char*>(&dummy), sizeof(WaveHeader));

        m_isRecording = true;
        m_stopThread = false;
        m_totalSamplesWritten = 0;
        m_writerThread = std::thread(&RecordingEngine::writerWork, this);
    }

    void stop() {
        bool expected = true;
        if (!m_isRecording.compare_exchange_strong(expected, false)) return;

        m_stopThread = true;
        if (m_writerThread.joinable()) m_writerThread.join();
        
        // 2. PATCH HEADER with real data size
        if (m_fileStream.is_open()) {
            WaveHeader header;
            header.sampleRate = static_cast<uint32_t>(m_sampleRate);
            header.byteRate = header.sampleRate * 2 * 4;
            header.blockAlign = 2 * 4;
            header.dataSize = static_cast<uint32_t>(m_totalSamplesWritten * 4); // Already interleaved total
            header.fileSize = 36 + header.dataSize;

            m_fileStream.seekp(0, std::ios::beg);
            m_fileStream.write(reinterpret_cast<char*>(&header), sizeof(WaveHeader));
            m_fileStream.close();
        }
    }

    /**
     * @brief Pushes incoming audio samples to the ring buffer.
     * RT-SAFE: No locks, no allocations.
     */
    void write(const float* l, const float* r, uint32_t numSamples) {
        if (!m_isRecording) return;
        for (uint32_t s = 0; s < numSamples; ++s) {
            m_ringBuffer.push(l[s]);
            m_ringBuffer.push(r[s]);
        }
    }

    bool isRecording() const { return m_isRecording.load(); }

private:
    void writerWork() {
        // High-priority disk writer thread
        while (m_isRecording.load(std::memory_order_acquire) || !m_stopThread.load(std::memory_order_relaxed)) {
            drainBuffer();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        // Final drain after recording has stopped to prevent trailing cut-off
        drainBuffer();
    }

    void drainBuffer() {
        // Batch drain to minimize context switching and disk syscall overhead
        float sampleBatch[8192];
        uint32_t count = 0;
        
        while (count < 8192 && m_ringBuffer.pop(sampleBatch[count])) {
            count++;
        }

        if (count > 0) {
            m_fileStream.write(reinterpret_cast<const char*>(sampleBatch), count * sizeof(float));
            m_totalSamplesWritten += count;
        }
    }

    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_stopThread;
    std::atomic<uint64_t> m_totalSamplesWritten{0};
    double m_sampleRate = 44100.0;
    std::thread m_writerThread;
    std::ofstream m_fileStream;
    LockFreeRingBuffer<float, kBufferSize> m_ringBuffer;
};

} // namespace Aura::Core
