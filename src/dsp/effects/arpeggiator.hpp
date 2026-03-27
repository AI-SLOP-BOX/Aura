#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @class Arpeggiator
 * @brief Professional MIDI Arpeggiator with Note-Lifespan management.
 * HONEST FIX: Prevents stuck notes (ghost notes) by tracking and sending 
 * Note-Off messages before each new trigger. Supports 1/16th BPM Sync.
 */
class Arpeggiator : public IProcessor {
public:
    enum class Mode { Up, Down, Range, Random };

    Arpeggiator(double sr = 44100.0) : m_sampleRate(sr) { reset(); }

    void prepareToPlay(double sr, uint32_t bs) override { m_sampleRate = sr; }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;
        
        // 1. DYNAMICALLY CAPTURE HELD NOTES
        Core::MidiBuffer::Iterator it{midi};
        uint8_t data[3]; uint32_t size; uint32_t offset;
        while (it.getNextEvent(offset, data, size)) {
            uint8_t status = data[0] & 0xF0;
            if (status == 0x90 && data[2] > 0) {
                if (std::find(m_heldNotes.begin(), m_heldNotes.end(), data[1]) == m_heldNotes.end())
                    m_heldNotes.push_back(data[1]);
            } else if (status == 0x80 || (status == 0x90 && data[2] == 0)) {
                auto nit = std::find(m_heldNotes.begin(), m_heldNotes.end(), data[1]);
                if (nit != m_heldNotes.end()) m_heldNotes.erase(nit);
            }
        }
        
        if (m_heldNotes.empty()) {
            killActiveNote(midi, 0);
            return;
        }
        std::sort(m_heldNotes.begin(), m_heldNotes.end());

        // 2. PATTERN SYNC (1/16th Note resolution)
        double samplesPer16th = (60.0 / context.bpm) * context.sampleRate / 4.0;
        uint64_t currentSample = context.playhead;
        uint32_t numSamples = buffer.getNumSamples();

        // Check if a trigger point exists within this buffer
        uint64_t nextTriggerSample = (static_cast<uint64_t>(currentSample / samplesPer16th) + 1) * samplesPer16th;
        
        if (nextTriggerSample >= currentSample && nextTriggerSample < (currentSample + numSamples)) {
            uint32_t triggerOffset = static_cast<uint32_t>(nextTriggerSample - currentSample);
            
            // --- PROFESSIONAL KILL PREVIOUS ---
            killActiveNote(midi, triggerOffset);

            // Trigger New Note
            m_stepCounter = (m_stepCounter + 1) % m_heldNotes.size();
            m_activeNote = m_heldNotes[m_stepCounter];
            
            uint8_t noteOn[3] = {0x90, m_activeNote, 100};
            midi.addEvent(triggerOffset, noteOn, 3);
        }
    }

    void reset() override {
        m_heldNotes.clear();
        m_activeNote = 0xFF;
        m_stepCounter = 0;
    }

private:
    void killActiveNote(Core::MidiBuffer& midi, uint32_t offset) {
        if (m_activeNote != 0xFF) {
            uint8_t noteOff[3] = {0x80, m_activeNote, 0};
            midi.addEvent(offset, noteOff, 3);
            m_activeNote = 0xFF;
        }
    }

    double m_sampleRate;
    std::vector<uint8_t> m_heldNotes;
    uint8_t m_activeNote = 0xFF; // Sentinal for 'None'
    uint32_t m_stepCounter = 0;
    Mode m_mode = Mode::Up;
};

} // namespace Aura::DSP::Effects
