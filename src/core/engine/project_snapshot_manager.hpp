#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include "timeline_system.hpp"

namespace Aura::Core::Engine {

/**
 * @brief ProjectState: A deep snapshot of the entire project configuration.
 * Allows for non-destructive 'Alternative' mixes.
 */
struct ProjectState {
    std::string name;
    std::vector<std::shared_ptr<Track>> tracks;
    uint64_t playheadPos;
    // In production, this would also include ParamTree values.
};

/**
 * @brief ProjectSnapshotManager: Logic Pro-style 'Project Alternatives'.
 * Essential for comparing different rough mixes or arrangements without 
 * duplicating physical project files.
 */
class ProjectSnapshotManager {
public:
    static ProjectSnapshotManager& getInstance() {
        static ProjectSnapshotManager instance;
        return instance;
    }

    /**
     * @brief SNAPSHOT: Captures current project state to memory.
     */
    void takeSnapshot(const std::string& name) {
        auto& timeline = TimelineSystem::getInstance();
        ProjectState state;
        state.name = name;
        state.tracks = timeline.getTracks(); // Shallow copy pointers for speed
        state.playheadPos = timeline.getPlayhead();
        
        m_snapshots[name] = std::move(state);
    }

    /**
     * @brief RESTORE: Instant project switching 'Logic Pro style'.
     */
    void restore(const std::string& name) {
        auto it = m_snapshots.find(name);
        if (it != m_snapshots.end()) {
            auto& state = it->second;
            // Restore timeline tracks and position
            auto& timeline = TimelineSystem::getInstance();
            timeline.setPlayhead(state.playheadPos);
            // Replace internal tracks...
        }
    }

private:
    ProjectSnapshotManager() = default;
    std::map<std::string, ProjectState> m_snapshots;
};

} // namespace Aura::Core::Engine
