#pragma once

#include <vector>
#include <algorithm>
#include <random>
#include "../midi_region.hpp"

namespace Aura::Core::Engine {

/**
 * @brief MidiTransformer: Professional Logical MIDI Editor.
 * Standard for batch processing (Logic Pro / Cubase).
 */
class MidiTransformer {
public:
    struct Filter {
        int minPitch = 0, maxPitch = 127;
        int minVel = 0, maxVel = 127;
        double minLen = 0.0, maxLen = 1000.0;
    };

    /**
     * @brief Batch Process: Applied to a selection of notes.
     */
    static void transform(std::vector<MIDINote>& notes, const Filter& f, 
                          int pitchOffset, float velScale, float humanizeAmount) {
        std::default_random_engine gen;
        std::uniform_real_distribution<float> dist(-humanizeAmount, humanizeAmount);

        for (auto& n : notes) {
            // 1. FILTERING
            if (n.pitch < f.minPitch || n.pitch > f.maxPitch) continue;
            if (n.velocity < f.minVel || n.velocity > f.maxVel) continue;
            if (n.lengthBeat < f.minLen || n.lengthBeat > f.maxLen) continue;

            // 2. OPERATIONS
            n.pitch = std::clamp(n.pitch + pitchOffset, 0, 127);
            n.velocity = std::clamp(static_cast<int>(n.velocity * velScale), 1, 127);
            
            // 3. HUMANIZE (Timing)
            if (humanizeAmount > 0.0f) {
                n.startBeat += dist(gen);
            }
        }
    }

    /**
     * @brief LEGATO: Extends note lengths to touch the next note.
     */
    static void applyLegato(std::vector<MIDINote>& notes) {
        if (notes.empty()) return;
        std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
            return a.startBeat < b.startBeat;
        });

        for (size_t i = 0; i < notes.size() - 1; ++i) {
            notes[i].lengthBeat = notes[i+1].startBeat - notes[i].startBeat;
        }
    }
};

} // namespace Aura::Core::Engine
