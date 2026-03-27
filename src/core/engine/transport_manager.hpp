#pragma once

#include <atomic>
#include <cstdint>

namespace Aura::Core::Engine {

/**
 * @brief TransportManager: Orchestrates Playback, Looping, and Recording.
 * Essential Logic Pro function for "Cycle" and "Punch-In" workflows.
 */
class TransportManager {
public:
    static TransportManager& getInstance() {
        static TransportManager instance;
        return instance;
    }

    struct CycleRange {
        uint64_t startSamples;
        uint64_t endSamples;
        bool isActive = false;
    };

    /**
     * @brief Advances the playhead and handles looping if cycle is active.
     */
    uint64_t advance(uint64_t current, uint32_t samplesToAdd) {
        uint64_t next = current + samplesToAdd;
        
        if (m_cycle.isActive && next >= m_cycle.endSamples) {
            // Logic Pro Cycle Jump
            uint64_t overflow = next - m_cycle.endSamples;
            return m_cycle.startSamples + overflow;
        }
        
        return next;
    }

    void setCycle(uint64_t start, uint64_t end, bool active) {
        m_cycle = {start, end, active};
    }

    void setRecording(bool recording) { m_isRecording.store(recording); }
    bool isRecording() const { return m_isRecording.load(); }

private:
    TransportManager() = default;

    CycleRange m_cycle;
    std::atomic<bool> m_isRecording{false};
};

} // namespace Aura::Core::Engine
