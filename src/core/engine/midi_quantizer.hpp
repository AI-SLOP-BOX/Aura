#pragma once
#include <vector>
#include <algorithm>
#include "../midi_region.hpp"

namespace Aura::Core::Engine {

/**
 * @brief MidiQuantizer: Professional Logic Pro-style grid-alignment.
 */
class MidiQuantizer {
public:
    enum class Resolution { Q1_4, Q1_8, Q1_16, Q1_32 };

    static void quantize(std::vector<MIDINote>& notes, Resolution res, float swing, float strength) {
        double grid = 0.25; // Default 1/16
        switch (res) {
            case Resolution::Q1_4: grid = 1.0; break;
            case Resolution::Q1_8: grid = 0.5; break;
            case Resolution::Q1_16: grid = 0.25; break;
            case Resolution::Q1_32: grid = 0.125; break;
        }

        for (auto& n : notes) {
            double target = std::round(n.startBeat / grid) * grid;
            
            // Swing Logic: Shift off-beats
            bool isOffBeat = (static_cast<int>(std::round(target / grid)) % 2) != 0;
            if (isOffBeat) target += grid * (swing * 0.5);

            // Blend based on Quantize Strength
            n.startBeat = n.startBeat + (target - n.startBeat) * std::clamp(strength, 0.0f, 1.0f);
        }
    }
};

} // namespace Aura::Core::Engine
