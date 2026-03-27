#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

class ChordTrigger : public IProcessor {
public:
    ChordTrigger() {
        m_chordIntervals = {0, 4, 7}; // Major Triad
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {}
    /**
     * @brief PROCESS: Injects chord notes with realistic "Strumming" and Velocity Scaling.
     * HONEST FIX: Prevents "Machine Gun" chords by adding micro-delays and expressive velocity tiers.
     */
    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        double sampleRate = context.sampleRate;
        uint32_t strumSamples = static_cast<uint32_t>((m_strumMs / 1000.0) * sampleRate);

        auto events = midi.getEvents();
        Core::MidiBuffer outputBuffer;
        
        for (const auto& ev : events) {
            uint8_t status = ev.data[0] & 0xF0;
            if (status == 0x90 || status == 0x80) {
                uint8_t rootNote = ev.data[1];
                uint8_t rootVel = ev.data[2];

                for (size_t i = 0; i < m_chordIntervals.size(); ++i) {
                    uint8_t chordNote = std::clamp(rootNote + m_chordIntervals[i], 0, 127);
                    
                    // Logic Pro 11 Expression: Velocity scaling (top notes slightly softer)
                    float velFactor = 1.0f - (i * 0.05f); 
                    uint8_t scaledVel = std::clamp(int(rootVel * velFactor), 1, 127);
                    
                    // Realistic Strumming: Delayed sample offset
                    uint32_t offset = ev.sampleOffset + (i * strumSamples);
                    if (status == 0x80) offset = ev.sampleOffset; // Release all at once or strum? Usually all.

                    uint8_t data[3] = {status, chordNote, scaledVel};
                    outputBuffer.addEvent(offset, data, 3);
                }
            } else {
                outputBuffer.addEvent(ev.sampleOffset, ev.data, ev.size);
            }
        }
        midi = std::move(outputBuffer);
    }

private:
    std::vector<int> m_chordIntervals;
    float m_strumMs = 15.0f; // Typical guitar strum delay
};

} // namespace Aura::DSP::Effects
