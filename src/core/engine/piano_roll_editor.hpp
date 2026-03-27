#pragma once

#include <vector>
#include <memory>
#include <set>
#include "../midi_region.hpp"

namespace Aura::Core::Engine {

/**
 * @brief PianoRollEditor: Orchestrates MIDI note editing logic.
 * Handles selection, deletion, and high-precision note positioning.
 */
class PianoRollEditor {
public:
    static PianoRollEditor& getInstance() {
        static PianoRollEditor instance;
        return instance;
    }

    void setRegion(std::shared_ptr<MIDIRegion> region) {
        m_region = region;
        m_selection.clear();
    }

    /**
     * @brief Adds a new note to the active MIDI region.
     */
    void addNote(uint8_t pitch, uint8_t vel, double beat, double len) {
        if (!m_region) return;
        MIDINote note{m_nextNoteId++, pitch, vel, beat, len};
        m_region->addNote(std::move(note));
    }

    /**
     * @brief Removes all currently selected notes.
     */
    void deleteSelected() {
        if (!m_region || m_selection.empty()) return;
        for (uint32_t noteId : m_selection) {
            m_region->removeNote(noteId);
        }
        m_selection.clear();
    }

    /**
     * @brief Moves all selected notes by a specified delta in beats and semitones.
     */
    void moveSelected(double beatDelta, int pitchDelta) {
        if (!m_region) return;
        for (auto& note : m_region->getNotes()) {
            if (m_selection.count(note.id)) {
                note.startBeat = std::max(0.0, note.startBeat + beatDelta);
                int newPitch = static_cast<int>(note.pitch) + pitchDelta;
                note.pitch = static_cast<uint8_t>(std::clamp(newPitch, 0, 127));
            }
        }
    }

    void selectNote(uint32_t noteId, bool multiSelect = false) {
        if (!multiSelect) m_selection.clear();
        m_selection.insert(noteId);
    }

    const std::set<uint32_t>& getSelection() const { return m_selection; }

private:
    PianoRollEditor() = default;
    
    std::shared_ptr<MIDIRegion> m_region;
    std::set<uint32_t> m_selection; // Note ID collection
    uint32_t m_nextNoteId = 1000;
};

} // namespace Aura::Core::Engine
