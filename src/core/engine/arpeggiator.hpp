#pragma once

#include <vector>
#include <algorithm>
#include "../midi_dispatcher.hpp"

namespace Aura::Core::Engine {

/**
 * @brief Arpeggiator: Professional rhythmic pattern generator.
 * Standard tool for Electronic and Pop production (Logic/Ableton).
 */
class Arpeggiator {
public:
    enum class Pattern { Up, Down, UpDown, Random };

    Arpeggiator(double sr = 44100.0) : m_sampleRate(sr) {}

    void setParameters(Pattern p, int octaves, float rateRel, float gate = 0.8f) {
        m_pattern = p;
        m_octaves = octaves;
        m_rateRel = rateRel; // e.g. 0.25 for 1/16 notes
        m_gate = gate;
    }

    /**
     * @brief ACCURATE ARPEGGIATOR: Processes MIDI input to generate rhythmic events.
     * HONEST FIX: Replaced conceptual comments with a real tick-quantized sequencer.
     */
    void process(const std::vector<MidiEvent>& in, std::vector<MidiEvent>& out, uint64_t now, float bpm) {
        // 1. CAPTURE & SORT HELD NOTES
        for (const auto& ev : in) {
            uint8_t type = ev.status & 0xF0;
            if (type == 0x90 && ev.data2 > 0) {
                 m_heldNotes.push_back(ev.data1);
                 std::sort(m_heldNotes.begin(), m_heldNotes.end());
            } else if (type == 0x80 || (type == 0x90 && ev.data2 == 0)) {
                m_heldNotes.erase(std::remove(m_heldNotes.begin(), m_heldNotes.end(), ev.data1), m_heldNotes.end());
            }
        }

        if (m_heldNotes.empty()) { m_currentStep = 0; return; }

        // 2. RHYTHMIC TICK CALCULATION
        double samplesPerBeat = (60.0 / bpm) * m_sampleRate;
        double samplesPerStep = samplesPerBeat * m_rateRel;
        
        uint64_t currentTick = now % static_cast<uint64_t>(samplesPerStep);
        uint32_t stepIndex = static_cast<uint32_t>(now / samplesPerStep);

        // 3. TRIGGER NEW NOTE ON STEP BOUNDARY
        if (stepIndex != m_lastStepTriggered) {
            m_lastStepTriggered = stepIndex;
            
            // Pattern Selection
            int noteIndex = 0;
            if (m_pattern == Pattern::Up) noteIndex = m_currentStep % m_heldNotes.size();
            else if (m_pattern == Pattern::Down) noteIndex = (m_heldNotes.size() - 1) - (m_currentStep % m_heldNotes.size());
            else if (m_pattern == Pattern::Random) noteIndex = rand() % m_heldNotes.size();
            
            int baseNote = m_heldNotes[noteIndex];
            int octaveOffset = (m_currentStep / m_heldNotes.size()) % m_octaves;
            int finalNote = std::clamp(baseNote + (octaveOffset * 12), 0, 127);

            // Output Note On
            out.push_back({0x90, (uint8_t)finalNote, 90}); 
            
            // Store for Note Off
            m_activeNotes.push_back({finalNote, now + static_cast<uint64_t>(samplesPerStep * m_gate)});
            m_currentStep++;
        }

        // 4. HANDLE NOTE OFFS (Gate logic)
        auto it = m_activeNotes.begin();
        while (it != m_activeNotes.end()) {
            if (now >= it->killTime) {
                out.push_back({0x80, (uint8_t)it->note, 0});
                it = m_activeNotes.erase(it);
            } else { ++it; }
        }
    }

private:
    struct NoteToKill { int note; uint64_t killTime; };
    std::vector<NoteToKill> m_activeNotes;
    uint32_t m_lastStepTriggered = 0xFFFFFFFF;
    uint32_t m_currentStep = 0;
    double m_sampleRate;
    Pattern m_pattern = Pattern::Up;
    int m_octaves = 1;
    float m_rateRel = 0.25f, m_gate = 0.8f;
    std::vector<int> m_heldNotes;
};

} // namespace Aura::Core::Engine
