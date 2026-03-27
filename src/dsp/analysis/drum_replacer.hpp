#pragma once

#include <vector>
#include <string>
#include "transient_detector.hpp"

namespace Aura::DSP::Analysis {

/**
 * @brief DrumReplacer: Audio-to-MIDI Trigger System.
 * Converts drum recording peaks into MIDI notes with sample-level phase alignment.
 * Essential for professional 'Layering' - reinforcing weak drums with studio samples.
 */
class DrumReplacer {
public:
    struct TriggerEvent {
        uint64_t samplePosition;
        float velocity;
        uint8_t midiNote;
    };

    /**
     * @brief DRUM DETECTION: Converts transient peaks into MIDI triggers.
     */
    static std::vector<TriggerEvent> convertToMidi(const float* buffer, size_t size, double sr, uint8_t targetNote = 36) {
        TransientDetector detector(sr);
        auto peaks = detector.detect(buffer, size, 0.4f); // Mid-sensitivity
        
        std::vector<TriggerEvent> triggers;
        for (auto p : peaks) {
            float peakVal = std::abs(buffer[p]);
            float vel = std::clamp(peakVal * 127.0f, 1.0f, 127.0f);
            triggers.push_back({p, vel, targetNote});
        }
        
        return triggers;
    }

    /**
     * @brief Generates a raw MIDI byte stream from the detected triggers.
     */
    static std::vector<uint8_t> generateMidiStream(const std::vector<TriggerEvent>& triggers) {
        std::vector<uint8_t> stream;
        for (const auto& t : triggers) {
            // Note ON (Status 0x90, Note, Velocity)
            stream.push_back(0x90);
            stream.push_back(t.midiNote);
            stream.push_back(static_cast<uint8_t>(t.velocity));
        }
        return stream;
    }
};

} // namespace Aura::DSP::Analysis
