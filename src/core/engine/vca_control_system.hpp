#pragma once

#include <vector>
#include <map>
#include <string>
#include "track.hpp"

namespace Aura::Core::Engine {

/**
 * @brief VCAGroup: A collection of tracks controlled by a single master fader.
 */
struct VCAGroup {
    uint32_t id;
    std::string name;
    float masterGain = 1.0f;
    std::vector<uint32_t> trackIds;
};

/**
 * @brief VCAControlSystem: Professional Large-Scale Console Workflow.
 * Standard for mixing projects with 100+ tracks (SSL/Neve console style).
 */
class VCAControlSystem {
public:
    static VCAControlSystem& getInstance() { static VCAControlSystem i; return i; }

    /**
     * @brief CREATE GROUP: Designates a master VCA fader for a set of tracks.
     */
    void createGroup(const std::string& name, const std::vector<uint32_t>& ids) {
        uint32_t newId = static_cast<uint32_t>(m_groups.size());
        m_groups[newId] = { newId, name, 1.0f, ids };
    }

    /**
     * @brief SET GAIN: Cascades gain reduction to all slave tracks.
     * Unlike Audio Busses, VCA affects the track faders directly (Pre-Post sends).
     */
    void setGroupGain(uint32_t groupId, float gain) {
        if (!m_groups.count(groupId)) return;
        m_groups[groupId].masterGain = gain;
        
        // (Conceptual update of TimelineSystem's track gains)
    }

    /**
     * @brief GET FINAL GAIN: Calculates the combined (Track + VCA) level.
     */
    float resolveTrackGain(uint32_t trackId, float baseGain) {
        float multiplier = 1.0f;
        for (const auto& [id, g] : m_groups) {
            for (uint32_t tid : g.trackIds) {
                if (tid == trackId) multiplier *= g.masterGain;
            }
        }
        return baseGain * multiplier;
    }

private:
    VCAControlSystem() = default;
    std::map<uint32_t, VCAGroup> m_groups;
};

} // namespace Aura::Core::Engine
