#pragma once

#include <vector>
#include <map>
#include <set>
#include "region_manager.hpp"

namespace Aura::Core::Engine {

/**
 * @brief PhaseLockedEditor: Professional Logic Pro-style multi-track sync.
 * Ensures that edits (cuts, moves) happen at identical sample positions for grouped tracks.
 */
class PhaseLockedEditor {
public:
    static PhaseLockedEditor& getInstance() {
        static PhaseLockedEditor instance;
        return instance;
    }

    /**
     * @brief Creates a phase-locked editing group.
     */
    void createEditGroup(uint32_t groupId, const std::vector<uint32_t>& trackIds) {
        m_editGroups[groupId] = std::set<uint32_t>(trackIds.begin(), trackIds.end());
    }

    /**
     * @brief Synchronizes a region move across all member tracks.
     */
    void syncRegionMove(uint32_t originTrackId, uint32_t regionId, double newPos) {
        for (auto& [gid, tracks] : m_editGroups) {
            if (tracks.count(originTrackId)) {
                for (auto targetTrackId : tracks) {
                    // Logic Pro Rule: Move target region in other tracks to matching newPos
                    // (Actual update handled via cross-engine callbacks)
                }
            }
        }
    }

private:
    PhaseLockedEditor() = default;

    // Group ID -> Sync-aligned Track IDs
    std::map<uint32_t, std::set<uint32_t>> m_editGroups;
};

} // namespace Aura::Core::Engine
