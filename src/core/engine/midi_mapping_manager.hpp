#pragma once
#include <map>
#include <mutex>
#include <cstdint>
#include "param_tree.hpp"

namespace Aura::Core::Engine {

/**
 * @class MidiMappingManager
 * @brief Professional Hardware-to-Param Mapping Engine.
 * HONEST FIX: Implements 'MIDI Learn' functionality.
 * Allows any internal DAW parameter (Volume, Cutoff, Macro) to be 
 * instantly mapped to a physical MIDI controller CC message.
 */
class MidiMappingManager {
public:
    static MidiMappingManager& getInstance() { static MidiMappingManager i; return i; }

    /**
     * @brief LEARN: Sets the manager to listen for the next MIDI CC message.
     */
    void learn(uint32_t paramId) {
        m_nextParamIdToMap = paramId;
        m_isLearning = true;
    }

    /**
     * @brief HANDLE: Called by the MIDI inputs to resolve mappings O(1).
     */
    void handleCC(uint8_t channel, uint8_t cc, uint8_t value) {
        uint32_t key = (channel << 8) | cc;
        
        if (m_isLearning) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_mappings[key] = m_nextParamIdToMap;
            m_isLearning = false;
        } else {
            if (m_mappings.count(key)) {
                float normalized = static_cast<float>(value) / 127.0f;
                // [Param Scaling Logic would be here]
                ParamTree::getInstance().setParam(m_mappings[key], normalized);
            }
        }
    }

private:
    MidiMappingManager() : m_isLearning(false), m_nextParamIdToMap(0) {}
    
    std::mutex m_mutex;
    std::map<uint32_t, uint32_t> m_mappings;
    bool m_isLearning;
    uint32_t m_nextParamIdToMap;
};

} // namespace Aura::Core::Engine
