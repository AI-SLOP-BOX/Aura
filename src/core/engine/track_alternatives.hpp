#pragma once

#include <vector>
#include <string>
#include <map>
#include "region_manager.hpp"

namespace Aura::Core::Engine {

/**
 * @brief TrackAlternatives: Versioning for region arrangements.
 * Allows switching between completely different edit patterns for a single track.
 * Iconic Logic Pro feature for creative workflows.
 */
class TrackAlternatives {
public:
    static TrackAlternatives& getInstance() {
        static TrackAlternatives instance;
        return instance;
    }

    struct Alternative {
        std::string name;
        std::vector<AudioRegion> regions;
    };

    /**
     * @brief Adds a new editing alternative for a track.
     */
    void createAlternative(uint32_t trackId, const std::string& name) {
        m_alts_by_track[trackId].push_back({name, {}});
    }

    /**
     * @brief Swaps the current arrangement with a different alternative.
     */
    void switchToAlternative(uint32_t trackId, size_t index) {
        if (index < m_alts_by_track[trackId].size()) {
            m_current_alt_index[trackId] = index;
        }
    }

    const Alternative* getActiveAlternative(uint32_t trackId) const {
        auto it = m_alts_by_track.find(trackId);
        if (it != m_alts_by_track.end()) {
            size_t idx = m_current_alt_index.at(trackId);
            return &it->second[idx];
        }
        return nullptr;
    }

private:
    TrackAlternatives() = default;

    // Track ID -> List of Alternatives (Arrangements)
    std::map<uint32_t, std::vector<Alternative>> m_alts_by_track;
    // Track ID -> Active Index
    std::map<uint32_t, size_t> m_current_alt_index;
};

} // namespace Aura::Core::Engine
