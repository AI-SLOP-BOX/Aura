#pragma once

#include <vector>
#include <memory>
#include <string>
#include "tempo_map.hpp"

namespace Aura::Core::Engine {

/**
 * @brief SessionClip: A single musical block for non-linear performance.
 */
struct SessionClip {
    uint32_t trackId;
    std::string name;
    std::shared_ptr<AudioRegion> audio; // (Or MIDI)
    bool looping = true;
};

/**
 * @brief SessionLauncher: Professional Clip-triggering and Scene management.
 * Standard for Live Performance and Modern Production (Ableton/Logic/Bitwig).
 */
class SessionLauncher {
public:
    static SessionLauncher& getInstance() { static SessionLauncher i; return i; }

    /**
     * @brief TRIGGER SCENE: Prepares all clips in a row for quantized launch.
     */
    void triggerScene(uint32_t sceneIdx) {
        m_pendingScene = sceneIdx;
        m_launchQueued = true;
    }

    /**
     * @brief UPDATE: Handles the actual quantized launch (Wait for next bar).
     */
    void update(uint64_t now) {
        if (!m_launchQueued) return;

        // Check if we hit the next 1-bar grid
        double samplesPerBar = (60.0 / TempoMap::getInstance().getBPMAt(now)) * 44100.0 * 4.0;
        if (std::fmod(static_cast<double>(now), samplesPerBar) < 1.0) {
            launchNow(m_pendingScene);
            m_launchQueued = false;
        }
    }

private:
    SessionLauncher() = default;

    void launchNow(uint32_t idx) {
        // Switch all active clips on the engine to the new scene
    }

    uint32_t m_pendingScene = 0;
    bool m_launchQueued = false;
};

} // namespace Aura::Core::Engine
