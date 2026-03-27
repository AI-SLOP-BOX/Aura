#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include "param_tree.hpp"

namespace Aura::Core::Engine {

/**
 * @brief MixSnapshot: A complete state of the mixing console.
 * Essential for comparing different mix approaches (A/B testing).
 */
struct MixSnapshot {
    std::string name;
    std::map<uint32_t, float> parameterStates; // ParamId -> Value
};

/**
 * @brief SnapshotManager: Pro-level scene recall system.
 */
class SnapshotManager {
public:
    static SnapshotManager& getInstance() { static SnapshotManager i; return i; }

    /**
     * @brief CAPTURE: Saves the current state of all parameters.
     */
    void takeSnapshot(const std::string& name) {
        MixSnapshot snap;
        snap.name = name;
        
        auto& pt = ParamTree::getInstance();
        for (uint32_t id = 0; id < 1024; ++id) { // Assume 1024 params for now
            float val = pt.getParam(id);
            snap.parameterStates[id] = val;
        }
        
        m_snapshots.push_back(std::move(snap));
    }

    /**
     * @brief RECALL: Instantly switches the console to a saved state.
     */
    void recallSnapshot(size_t index) {
        if (index >= m_snapshots.size()) return;
        
        const auto& snap = m_snapshots[index];
        auto& pt = ParamTree::getInstance();
        
        for (const auto& [id, val] : snap.parameterStates) {
            pt.setParam(id, val);
        }
    }

    const std::vector<MixSnapshot>& getSnapshots() const { return m_snapshots; }

private:
    SnapshotManager() = default;
    std::vector<MixSnapshot> m_snapshots;
};

} // namespace Aura::Core::Engine
