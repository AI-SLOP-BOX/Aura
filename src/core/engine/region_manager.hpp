#pragma once

#include <vector>
#include <string>
#include <map>

namespace Aura::Core::Engine {

/**
 * @brief AudioRegion: Professional non-destructive clip object.
 * Holds per-clip metadata like Gain and Mute state, independent of the track.
 */
struct AudioRegion {
    uint32_t id;
    std::string filePath;
    double samplePosition;
    double sampleLength;
    float clipGain = 1.0f; // 0dB default
    bool isMuted = false;
};

/**
 * @brief RegionManager: Orchestrates all non-destructive clip edits.
 * Critical for Ardour-style deep regional editing.
 */
class RegionManager {
public:
    static RegionManager& getInstance() {
        static RegionManager instance;
        return instance;
    }

    void addRegion(uint32_t trackId, const std::string& path, double pos, double len) {
        m_regions_by_track[trackId].push_back({static_cast<uint32_t>(m_regions_by_track[trackId].size()), path, pos, len, 1.0f, false});
    }

    void setClipGain(uint32_t trackId, uint32_t regionId, float gain) {
        if (regionId < m_regions_by_track[trackId].size()) {
            m_regions_by_track[trackId][regionId].clipGain = gain;
        }
    }

    const std::vector<AudioRegion>& getRegionsForTrack(uint32_t trackId) const {
        return m_regions_by_track.at(trackId);
    }

private:
    RegionManager() = default;

    // Track ID -> List of Regions
    std::map<uint32_t, std::vector<AudioRegion>> m_regions_by_track;
};

} // namespace Aura::Core::Engine
