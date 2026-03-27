#pragma once
#include <map>
#include <memory>
#include "../midi_region.hpp"

namespace Aura::Core::Engine {

/**
 * @brief MidiSequencer: Orchestrates MIDI performance and recording.
 */
class MidiSequencer {
public:
    static MidiSequencer& getInstance() {
        static MidiSequencer instance;
        return instance;
    }

    /**
     * @brief Records a MIDI event into a specific region.
     */
    void recordNote(uint32_t regionId, uint8_t pitch, uint8_t velocity, double beat, double lengthBeats) {
        auto it = m_regions.find(regionId);
        if (it != m_regions.end()) {
            MIDINote note{m_nextId++, pitch, velocity, beat, lengthBeats};
            it->second->addNote(std::move(note));
        }
    }

    std::shared_ptr<MIDIRegion> getRegion(uint32_t id) {
        auto it = m_regions.find(id);
        return (it != m_regions.end()) ? it->second : nullptr;
    }

    void registerRegion(std::shared_ptr<MIDIRegion> region) {
        if (region) m_regions[region->getMeta().id] = region;
    }

private:
    MidiSequencer() = default;
    std::map<uint32_t, std::shared_ptr<MIDIRegion>> m_regions;
    uint32_t m_nextId = 5000;
};

} // namespace Aura::Core::Engine
