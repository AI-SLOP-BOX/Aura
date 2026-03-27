#pragma once

#include <vector>
#include <map>
#include <atomic>

namespace Aura::Core {

/**
 * @brief MidiLearnManager: Maps MIDI CC messages to synth/mixer parameters.
 * Addresses the "missing controller integration" from the review.
 */
class MidiLearnManager {
public:
    struct Mapping {
        uint8_t cc;
        std::atomic<float>* parameter;
    };

    /**
     * @brief Adds a new MIDI mapping.
     */
    void addMapping(uint8_t cc, std::atomic<float>* param) {
        m_mappings[cc] = param;
    }

    /**
     * @brief Processes an incoming MIDI CC message and updates the linked parameter.
     */
    void handleMidiCC(uint8_t cc, uint8_t value) {
        if (m_mappings.find(cc) != m_mappings.end()) {
            float normalized = static_cast<float>(value) / 127.0f;
            m_mappings[cc]->store(normalized);
        }
    }

private:
    std::map<uint8_t, std::atomic<float>*> m_mappings;
};

} // namespace Aura::Core
