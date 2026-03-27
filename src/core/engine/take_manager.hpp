#pragma once

#include <vector>
#include <string>
#include <map>

namespace Aura::Core::Engine {

/**
 * @brief AudioTake: A single recording attempt.
 */
struct AudioTake {
    uint32_t id;
    std::string filePath;
    double startSamples;
};

/**
 * @brief TakeManager: Orchestrates multiple recording takes for a single track segment.
 * Critical for "Comping" workflows in professional production.
 */
class TakeManager {
public:
    static TakeManager& getInstance() {
        static TakeManager instance;
        return instance;
    }

    /**
     * @brief Adds a new recording as a take to a specific lane.
     */
    void addTake(uint32_t trackId, const std::string& path, double start) {
        m_takes[trackId].push_back({static_cast<uint32_t>(m_takes[trackId].size()), path, start});
    }

    /**
     * @brief Returns the total number of takes for a track.
     */
    size_t getTakeCount(uint32_t trackId) const {
        auto it = m_takes.find(trackId);
        return (it != m_takes.end()) ? it->second.size() : 0;
    }

    const AudioTake* getTake(uint32_t trackId, uint32_t takeIndex) const {
        auto it = m_takes.find(trackId);
        if (it != m_takes.end() && takeIndex < it->second.size()) {
            return &it->second[takeIndex];
        }
        return nullptr;
    }

private:
    TakeManager() = default;

    // Track ID -> List of Takes
    std::map<uint32_t, std::vector<AudioTake>> m_takes;
};

} // namespace Aura::Core::Engine
