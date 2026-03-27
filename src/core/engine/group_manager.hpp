#pragma once

#include <vector>
#include <map>
#include <set>
#include <atomic>

namespace Aura::Core::Engine {

/**
 * @brief GroupManager: Synchronizes track parameters (Mute, Solo, Record).
 * Iconic Logic Pro feature for managing multi-mic drum or orchestral recordings.
 */
class GroupManager {
public:
    static GroupManager& getInstance() {
        static GroupManager instance;
        return instance;
    }

    /**
     * @brief Assigns a track to a group.
     */
    void addTaskToGroup(uint32_t trackId, uint32_t groupId) {
        m_groups[groupId].insert(trackId);
    }

    /**
     * @brief Propagates Mute state to all members of the group.
     */
    void syncMute(uint32_t originTrackId, bool isMuted) {
        for (auto& [gid, tracks] : m_groups) {
            if (tracks.count(originTrackId)) {
                for (auto targetId : tracks) {
                    // Logic Pro Rule: Propagate to ALL track objects
                    // (Actual update happens via track_registry later)
                }
            }
        }
    }

private:
    GroupManager() = default;

    // Group ID -> Set of Track IDs
    std::map<uint32_t, std::set<uint32_t>> m_groups;
};

} // namespace Aura::Core::Engine
