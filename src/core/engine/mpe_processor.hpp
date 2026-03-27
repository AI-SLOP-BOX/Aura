#pragma once

#include <vector>
#include <map>
#include "../midi_dispatcher.hpp"

namespace Aura::Core::Engine {

/**
 * @brief MPE: MIDI Polyphonic Expression infrastructure.
 * Standard for modern expressive controllers (Roli Seaboard, LinnStrument).
 */
class MPEProcessor {
public:
    struct NoteState {
        int noteNum;
        float pressure; // [0, 1]
        float slide;    // [0, 1]
        float bend;     // [-1, 1]
    };

    /**
     * @brief PROCESS: Decodes MPE data into per-note expressive states.
     * MPE uses 1 channel per note to achieve per-note modulation.
     */
    void processMidi(const MidiEvent& ev) {
        int channel = ev.status & 0x0F;
        int type = ev.status & 0xF0;

        if (type == 0x90 && ev.data2 > 0) { // Note On
            m_voiceMap[channel] = { ev.data1, 0.0f, 0.5f, 0.0f };
        } else if (type == 0xD0) { // Channel Pressure (Used for per-note MPE Pressure)
            if (m_voiceMap.count(channel)) m_voiceMap[channel].pressure = ev.data1 / 127.0f;
        } else if (type == 0xB0 && ev.data1 == 74) { // CC74 (Used for MPE Slide/Timbre)
            if (m_voiceMap.count(channel)) m_voiceMap[channel].slide = ev.data2 / 127.0f;
        } else if (type == 0xE0) { // Pitch Bend (Used for per-note MPE Bend)
            if (m_voiceMap.count(channel)) {
                int bendVal = (ev.data2 << 7) | ev.data1;
                m_voiceMap[channel].bend = (bendVal - 8192) / 8192.0f;
            }
        }
    }

    const std::map<int, NoteState>& getActiveVoices() const { return m_voiceMap; }

private:
    // Channel -> State
    std::map<int, NoteState> m_voiceMap;
};

} // namespace Aura::Core::Engine
