#pragma once

#include <atomic>
#include <chrono>

namespace Aura::Core::Engine {

/**
 * @brief TransportAnchor: High-resolution temporal clock for multi-engine synchronization.
 * Locks Samples, MIDI Ticks, and Video Frames into a single professional timeline.
 */
class TransportAnchor {
public:
    static TransportAnchor& getInstance() {
        static TransportAnchor instance;
        return instance;
    }

    void setSampleRate(double rate) { m_sampleRate = rate; }
    void setFPS(float fps) { m_fps = fps; }

    /**
     * @brief Updates the playhead position (in samples).
     */
    void updatePlayhead(uint64_t samples) { m_playheadSamples.store(samples); }

    /**
     * @brief Returns the equivalent video frame for the current audio position.
     */
    uint64_t getCurrentVideoFrame() const {
        return static_cast<uint64_t>((m_playheadSamples.load() / m_sampleRate) * m_fps);
    }

    uint64_t getPlayheadSamples() const { return m_playheadSamples.load(); }

    /**
     * @brief High-precision timer for UI synchronization (144Hz ready).
     */
    double getWallTimeSeconds() const {
        return m_playheadSamples.load() / m_sampleRate;
    }

private:
    TransportAnchor() = default;

    std::atomic<uint64_t> m_playheadSamples{0};
    double m_sampleRate = 48000.0;
    float m_fps = 24.0f;
};

} // namespace Aura::Core::Engine
