#pragma once

#include <vector>
#include <cstdint>

namespace Aura::Core {

/**
 * @brief MidiEvent: Sample-accurate MIDI data container.
 */
struct MidiEvent {
    uint32_t sampleOffset;
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};

/**
 * @brief MidiDispatcher: Orchestrates MIDI events across a render block.
 * Addresses the "lack of sample-accurate MIDI management" from the review.
 */
class MidiDispatcher {
public:
    void pushEvent(const MidiEvent& event) {
        m_events.push_back(event);
    }

    const std::vector<MidiEvent>& getEventsForBlock() const {
        return m_events;
    }

    void clear() { m_events.clear(); }

private:
    std::vector<MidiEvent> m_events;
};

} // namespace Aura::Core
