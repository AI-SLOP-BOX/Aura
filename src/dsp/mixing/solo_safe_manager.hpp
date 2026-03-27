#pragma once

#include <set>
#include <mutex>

namespace Aura::Core::DSP::Mixing {

/**
 * @brief SoloSafeManager: Professional "Solo Defeat" system.
 * Prevents Bus/Aux tracks from being muted when other tracks are turned to Solo.
 * Critical Logic Pro feature for reliable effect sub-mixing.
 */
class SoloSafeManager {
public:
    static SoloSafeManager& getInstance() {
        static SoloSafeManager instance;
        return instance;
    }

    /**
     * @brief Marks a track (usually a Reverb Aux) as "Solo Safe."
     */
    void setSoloSafe(uint32_t trackId, bool safe) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (safe) m_safeTracks.insert(trackId);
        else m_safeTracks.erase(trackId);
    }

    /**
     * @brief Determines if a track should stay audible during global Solo mode.
     */
    bool isProtected(uint32_t trackId) const {
        return m_safeTracks.count(trackId) > 0;
    }

private:
    SoloSafeManager() = default;

    std::set<uint32_t> m_safeTracks;
    mutable std::mutex m_mutex;
};

} // namespace Aura::Core::DSP::Mixing
