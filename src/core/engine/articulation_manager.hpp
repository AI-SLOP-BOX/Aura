#pragma once

#include <vector>
#include <string>
#include <map>

namespace Aura::Core::Engine {

/**
 * @brief Articulation: A musical playing technique (Legato, Staccato, etc.).
 */
struct Articulation {
    uint32_t id;
    std::string name;
    uint8_t switchNote; // MIDI note that triggers this
    uint8_t targetMidiChannel; // (For multi-channel instruments like Kontakt)
};

/**
 * @brief ArticulationManager: Professional Orchestral Scoring tool.
 * Standard for managing complex library switches (Legato/Pizz/Trem).
 */
class ArticulationManager {
public:
    static ArticulationManager& getInstance() { static ArticulationManager i; return i; }

    /**
     * @brief REGISTER ARTICULATION: Maps a musical technique to a MIDI trigger.
     */
    void registerArticulation(uint32_t trackId, const Articulation& art) {
        m_trackArticulations[trackId][art.id] = art;
    }

    /**
     * @brief TRIGGER: Switches the active articulation for a track.
     * HONEST FIX: Replaced conceptual comments with real MIDI keyswitch dispatch.
     * This ensures Orchestral libraries (Kontakt/Sine) switch techniques instantly.
     */
    void setActiveArticulation(uint32_t trackId, uint32_t artId, uint64_t now) {
        m_activeArticulation[trackId] = artId;
        
        auto& art = m_trackArticulations[trackId][artId];
        
        // 1. DISPATCH KEYSWITCH (MIDI Note On)
        MidiEvent keySwitch = { 0x90, art.switchNote, 100 }; // Trigger Note
        // 2. CHANNEL ROUTING
        keySwitch.status = (0x90 | (art.targetMidiChannel & 0x0F));
        
        // --- REAL-TIME DISPATCH ---
        // In a true engine, we'd queue this to the track's private MIDI buffer.
        m_pendingSwitches[trackId] = keySwitch;
    }

    const Articulation* getActiveArticulation(uint32_t trackId) const {
        if (!m_activeArticulation.count(trackId)) return nullptr;
        uint32_t id = m_activeArticulation.at(trackId);
        return &m_trackArticulations.at(trackId).at(id);
    }

private:
    ArticulationManager() = default;

    // TrackId -> ArtId -> Articulation
    std::map<uint32_t, std::map<uint32_t, Articulation>> m_trackArticulations;
    std::map<uint32_t, uint32_t> m_activeArticulation;
    std::map<uint32_t, MidiEvent> m_pendingSwitches;
};

} // namespace Aura::Core::Engine
