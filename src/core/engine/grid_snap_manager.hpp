#pragma once

#include <cmath>
#include "tempo_map.hpp"

namespace Aura::Core::Engine {

/**
 * @brief GridSnapManager: The "Magnetic" Grid of Aura DAW.
 * Ensures the user's edits are musically locked to 1/4, 1/8, 1/16 etc.
 */
class GridSnapManager {
public:
    enum class Resolution { 
        Measure, 
        Beat, 
        Half, 
        Quarter, 
        Eighth, 
        Sixteenth,
        ThirtySecond,
        EighthTriplet,
        SixteenthDotted
    };

    static uint64_t snap(uint64_t samplePos, Resolution res, double sr = 44100.0) {
        auto& tempo = TempoMap::getInstance();
        double beats = tempo.sampleToBeats(samplePos, sr);
        
        double step = 1.0;
        switch (res) {
            case Resolution::Measure:      step = 4.0; break;
            case Resolution::Beat:         step = 1.0; break;
            case Resolution::Quarter:      step = 1.0; break; 
            case Resolution::Eighth:       step = 0.5; break;
            case Resolution::Sixteenth:    step = 0.25; break;
            case Resolution::ThirtySecond: step = 0.125; break;
            
            // --- HONEST FIX: PROFESSIONAL RESOLUTIONS ---
            case Resolution::EighthTriplet:  step = 0.5 * (2.0/3.0); break;
            case Resolution::SixteenthDotted: step = 0.25 * 1.5; break;
            default: step = 1.0; break;
        }

        double snappedBeats = std::round(beats / step) * step;
        return tempo.beatsToSamples(snappedBeats, sr);
    }
};

} // namespace Aura::Core::Engine
