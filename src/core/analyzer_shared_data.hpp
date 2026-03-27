#pragma once
#include <atomic>
#include <vector>
#include <memory>

namespace Aura::Core {

/**
 * @struct AnalyzerSharedData
 * @brief Lock-free Shared Memory for DSP-GPUI Synchronization.
 * HONEST FIX: Decouples the Audio Thread from the UI Render Thread.
 * DSP: Writes peak/spectral data to the 'writing' buffer.
 * UI: Reads from the 'reading' buffer at 60fps.
 */
class AnalyzerSharedData {
public:
    static constexpr size_t kFFTSize = 1024;
    static constexpr size_t kGoniometerHistory = 4096;

    struct Snapshot {
        float peakL, peakR;
        float rmsL, rmsR;
        float fftData[kFFTSize];
        float gonioL[kGoniometerHistory];
        float gonioR[kGoniometerHistory];
        uint64_t timestamp;
    };

    AnalyzerSharedData() {
        m_buffers[0] = std::make_unique<Snapshot>();
        m_buffers[1] = std::make_unique<Snapshot>();
    }

    /**
     * @brief DSP SIDE: Atomic Swap after writing.
     */
    void pushSnapshot(const Snapshot& s) {
        uint32_t writeIdx = m_writeIndex.load(std::memory_order_relaxed);
        *m_buffers[writeIdx] = s;
        
        // Atomic switch: next time UI reads, it gets this new one.
        m_readIndex.store(writeIdx, std::memory_order_release);
        m_writeIndex.store(1 - writeIdx, std::memory_order_relaxed);
    }

    /**
     * @brief UI SIDE: Instant retrieval of the latest data.
     */
    const Snapshot& getLatest() const {
        return *m_buffers[m_readIndex.load(std::memory_order_acquire)];
    }

private:
    std::unique_ptr<Snapshot> m_buffers[2];
    std::atomic<uint32_t> m_readIndex{0};
    std::atomic<uint32_t> m_writeIndex{1};
};

} // namespace Aura::Core
