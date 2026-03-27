#pragma once

#include <vector>
#include <map>
#include <memory>

namespace Aura::Core::Engine {

/**
 * @brief MPEEvent: MIDI Polyphonic Expression data.
 * Features 5D control for each individual note.
 */
struct MPEEvent {
    uint8_t note;
    float pressure; // Z-axis
    float timbre;   // Y-axis (CC74)
    float pitchBend; // X-axis (High resolution)
};

/**
 * @brief MPEManager: Professional MIDI Polyphonic Expression.
 * Standard for modern expressive performance (ROLI Seaboard style).
 */
class MPEManager {
public:
    static MPEManager& getInstance() { static MPEManager i; return i; }

    /**
     * @brief HANDLE MIDI: Decodes incoming multi-channel MPE messages.
     * MPE uses MIDI channels 2-16 for individual note control.
     */
    void processMidiChannel(uint8_t channel, uint8_t controller, uint8_t value) {
        if (channel < 2 || channel > 16) return;

        uint32_t voiceIdx = channel - 2;
        if (controller == 74) {
            m_voices[voiceIdx].timbre = value / 127.0f;
        } else if (controller == 130) { // Aftertouch / Pressure
            m_voices[voiceIdx].pressure = value / 127.0f;
        }
    }

    /**
     * @brief PITCH BEND: High-resolution per-note pitch control.
     */
    void processPitchBend(uint8_t channel, int bendValue) {
        if (channel < 2 || channel > 16) return;
        m_voices[channel - 2].pitchBend = (bendValue - 8192) / 8192.0f;
    }

    const MPEEvent* getVoiceData(uint8_t channel) const {
        if (channel < 2 || channel > 16) return nullptr;
        return &m_voices[channel - 2];
    }

private:
    MPEManager() { m_voices.resize(15); }
    std::vector<MPEEvent> m_voices;
};

} // namespace Aura::Core::Engine
