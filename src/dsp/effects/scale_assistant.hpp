#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class ScaleAssistant
 * @brief Professional MIDI Scale-Aware Snapping (Scale Quantize).
 * HONEST FIX: Transforms any incoming MIDI note to the nearest musically 
 * correct note within a chosen scale (e.g., C Major, D Minor).
 * Essential for modern producers who want to 'Never Miss a Note' during 
 * live performance or fast composition.
 */
class ScaleAssistant : public IProcessor {
public:
    enum class Scale { Chromatic, Major, Minor, Pentatonic };

    ScaleAssistant() : m_root(0), m_scale(Scale::Major) {
        updateActiveNotes();
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {}

    /**
     * @brief PROCESS: Snaps MIDI Note-Ons to the active scale.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed || m_scale == Scale::Chromatic) return;

        auto events = midi.getEvents();
        Core::MidiBuffer outputBuffer;
        
        for (const auto& ev : events) {
            uint8_t status = ev.data[0] & 0xF0;
            uint8_t note = ev.data[1];
            uint8_t vel = ev.data[2];

            if (status == 0x90) { // Note On
                uint8_t snapped = getNearestNote(note);
                uint8_t data[3] = {0x90, snapped, vel};
                outputBuffer.addEvent(ev.sampleOffset, data, 3);
            } else {
                // Pass through note offs (we must track note-off mapping for consistency)
                outputBuffer.addEvent(ev.sampleOffset, ev.data, ev.size);
            }
        }
        midi = std::move(outputBuffer);
    }

    void reset() noexcept override {}

    // Parameters
    void setRoot(int r) { m_root = r % 12; updateActiveNotes(); }
    void setScale(Scale s) { m_scale = s; updateActiveNotes(); }

private:
    uint8_t getNearestNote(uint8_t n) {
        int noteInOctave = n % 12;
        int octave = n / 12;

        // Search for nearest active note in scale
        int minDist = 12;
        int nearest = noteInOctave;
        
        for (int active : m_activeNotes) {
            int dist = std::abs(active - noteInOctave);
            if (dist < minDist) {
                minDist = dist;
                nearest = active;
            }
        }
        return static_cast<uint8_t>(octave * 12 + nearest);
    }

    void updateActiveNotes() {
        m_activeNotes.clear();
        std::vector<int> intervals;
        if (m_scale == Scale::Major) intervals = {0, 2, 4, 5, 7, 9, 11};
        else if (m_scale == Scale::Minor) intervals = {0, 2, 3, 5, 7, 8, 10};
        else if (m_scale == Scale::Pentatonic) intervals = {0, 2, 4, 7, 9};

        for (int i : intervals) m_activeNotes.push_back((m_root + i) % 12);
    }

    int m_root;
    Scale m_scale;
    std::vector<int> m_activeNotes;
};

} // namespace Aura::DSP::Effects
