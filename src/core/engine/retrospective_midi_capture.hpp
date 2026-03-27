#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include "midi_sequencer.hpp"

namespace Aura::Core::Engine {

/**
 * @brief RetrospectiveMidiCapture: Always-on "Shadow" recording for MIDI.
 * Iconic Logic Pro feature that allows "Shift-R" to salvage captures performed before record-on.
 */
class RetrospectiveMidiCapture {
public:
    static RetrospectiveMidiCapture& getInstance() {
        static RetrospectiveMidiCapture instance;
        return instance;
    }

    /**
     * @brief Shadow recording: Always buffers incoming MIDI.
     * Point 4: Automatically called from UnifiedEngine for all active tracks.
     */
    void bufferMidi(uint32_t trackId, const MidiBuffer& midi, uint64_t playhead) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& ev : midi.getEvents()) {
            double beat = TempoMap::getInstance().samplesToBeats(playhead + ev.offset, 44100.0);
            
            if (ev.type == MidiEvent::Type::NoteOn) {
                MidiNote n;
                n.pitch = ev.pitch; n.velocity = ev.velocity;
                n.startBeat = beat; n.lengthBeats = 1e-6; // Tentative
                n.trackId = trackId;
                m_activeNotes[{trackId, ev.pitch}] = n;
            } else if (ev.type == MidiEvent::Type::NoteOff) {
                auto it = m_activeNotes.find({trackId, ev.pitch});
                if (it != m_activeNotes.end()) {
                    it->second.lengthBeats = std::max(0.01, beat - it->second.startBeat);
                    m_captureBuffer.push_back(it->second);
                    m_activeNotes.erase(it);
                    if (m_captureBuffer.size() > m_maxBuffer) m_captureBuffer.pop_front();
                }
            }
        }
    }

    std::vector<MidiNote> flushCapture() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<MidiNote> capture(m_captureBuffer.begin(), m_captureBuffer.end());
        m_captureBuffer.clear(); m_activeNotes.clear();
        return capture;
    }

private:
    RetrospectiveMidiCapture() = default;

    struct NoteKey { 
        uint32_t tid; int p; 
        bool operator<(const NoteKey& o) const { return tid < o.tid || (tid == o.tid && p < o.p); }
    };
    std::deque<MidiNote> m_captureBuffer;
    std::map<NoteKey, MidiNote> m_activeNotes;
    const size_t m_maxBuffer = 5000; 
    std::mutex m_mutex;
};

} // namespace Aura::Core::Engine
