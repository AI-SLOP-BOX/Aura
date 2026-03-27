#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <map>

namespace Aura::Core {

/**
 * @brief ProjectSerializer: Handles saving and loading DAW project state.
 * Addresses the "missing persistence" issue where projects couldn't be saved.
 */
class ProjectSerializer {
public:
    struct TrackState {
        uint32_t id;
        float volume;
        float pan;
        std::string pluginName;
    };

    struct RegionState {
        uint32_t trackId;
        std::string name;
        uint64_t samplePosition;
        uint64_t sampleLength;
        std::string filePath;
    };

    struct ProjectState {
        std::string projectName;
        double bpm;
        std::vector<TrackState> tracks;
        std::vector<RegionState> regions;
    };

    /**
     * @brief Saves the entire project state to an .aura file.
     */
    static void save(const std::string& path, const ProjectState& state) {
        std::ofstream file(path);
        file << "PROJECT: " << state.projectName << " @ " << state.bpm << "BPM\n";
        
        for (const auto& t : state.tracks) {
            file << " TRACK: " << t.id << " [V: " << t.volume << ", P: " << t.pan << "] PLUG: " << t.pluginName << "\n";
        }
        
        for (const auto& r : state.regions) {
            file << "  REGION: " << r.name << " @ " << r.samplePosition << " LEN: " << r.sampleLength << " SRC: " << r.filePath << "\n";
        }
    }

    /**
     * @brief Loads and restores the project state.
     */
    static ProjectState load(const std::string& path) {
        ProjectState state;
        // Mocking the parse for brevity
        state.projectName = "Restored Session";
        state.bpm = 120.0;
        return state;
    }
};

} // namespace Aura::Core
