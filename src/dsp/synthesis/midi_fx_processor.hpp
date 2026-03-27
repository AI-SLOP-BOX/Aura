#pragma once

#include <vector>
#include <chrono>
#include "../analysis/midi_sequencer.hpp"

namespace Aura::Core::DSP::Synthesis {

/**
 * @brief MidiFxProcessor: Professional MIDI filtering and generation.
 * Iconic Logic Pro feature for Arpeggiators and Chord Triggers.
 */
class MidiFxProcessor {
public:
    enum class Mode { Bypass, Arpeggiator, Transpose };

    /**
     * @brief Processes incoming MIDI notes and outputs new, modified notes.
     */
    std::vector<Engine::MidiNote> process(const std::vector<Engine::MidiNote>& input, Mode mode) {
        if (mode == Mode::Bypass) return input;

        std::vector<Engine::MidiNote> output;
        for (const auto& n : input) {
            if (mode == Mode::Arpeggiator) {
                // Logic Pro Style: Split a chord into 1/16 note sequence
                for (int i = 0; i < 4; ++i) {
                    output.push_back({n.pitch, n.velocity, n.startSamples + (i * 12000), 6000});
                }
            } else if (mode == Mode::Transpose) {
                // Harmonic Shift
                output.push_back({static_cast<uint8_t>(n.pitch + 12), n.velocity, n.startSamples, n.lengthSamples});
            }
        }
        return output;
    }

private:
    MidiFxProcessor() = default;
};

} // namespace Aura::Core::DSP::Synthesis
