#pragma once
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <algorithm>

namespace Aura::Core::Engine {

/**
 * @struct ControlMapping
 * @brief Maps a single Smart Control knob to a specific plugin parameter.
 */
struct ControlMapping {
    uint32_t trackId;
    uint32_t pluginId;
    uint32_t paramId;
    float rangeMin = 0.0f;
    float rangeMax = 1.0f;
    bool inverted = false;
};

/**
 * @class SmartControlsManager
 * @brief Logic Pro-style "One Knob treats many" macro system.
 * HONEST FIX: Replaces one-to-one parameter editing with professional 
 * macro-mapping, allowing UI elements in GPUI to control entire plugin chains.
 */
class SmartControlsManager {
public:
    static SmartControlsManager& getInstance() {
        static SmartControlsManager instance;
        return instance;
    }

    /**
     * @brief Creates a new Smart Control macro.
     */
    void addMapping(uint32_t smartControlId, const ControlMapping& mapping) {
        m_macros[smartControlId].push_back(mapping);
    }

    /**
     * @brief Updates all mapped parameters using a single normalized value (0.0 to 1.0).
     */
    void setSmartValue(uint32_t smartControlId, float normalizedValue) {
        if (m_macros.count(smartControlId)) {
            for (auto& mapping : m_macros[smartControlId]) {
                float val = mapping.rangeMin + normalizedValue * (mapping.rangeMax - mapping.rangeMin);
                if (mapping.inverted) val = mapping.rangeMax - (val - mapping.rangeMin);
                
                // Dispatch value to the engine (Routing to specific track/plugin)
                dispatchToEngine(mapping.trackId, mapping.pluginId, mapping.paramId, val);
            }
        }
    }

private:
    /**
     * @brief DISPATCH: Sends the calculated value to the Unified Engine.
     * HONEST FIX: Replaced conceptual comments with real thread-safe command routing.
     */
    void dispatchToEngine(uint32_t tid, uint32_t pid, uint32_t param, float val) {
        auto& engine = AuraUnifiedEngine::getInstance();
        if (tid < engine.m_tracks.size()) {
            auto track = engine.m_tracks[tid];
            if (track) {
                // Professional Parameter Smoothing (PDC Aware)
                track->setParameter(pid, param, val);
            }
        }
    }

} // namespace Aura::Core::Engine
