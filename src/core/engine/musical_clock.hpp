#pragma once

#include <atomic>
#include <cmath>

namespace Aura::Core::Engine {

/**
 * @brief MusicalClock: The 'Conductor' of the DAW.
 * Handles high-precision conversion between audio samples and musical beats/bars.
 */
class MusicalClock {
public:
    static MusicalClock& getInstance() { static MusicalClock i; return i; }

    void setTempo(double bpm) { m_tempo = bpm; }
    void setTimeSignature(uint32_t num, uint32_t denom) { 
        m_numerator = num; m_denominator = denom; 
    }

    /**
     * @brief CONVERT: Samples -> Beats.
     * Essential for Delay sync and MIDI quantization.
     */
    double samplesToBeats(uint64_t samples, double sampleRate) const {
        double seconds = static_cast<double>(samples) / sampleRate;
        return (seconds * m_tempo) / 60.0;
    }

    /**
     * @brief GET CURRENT BAR: Calculation for UI and grid snapping.
     */
    uint32_t getBar(double beats) const {
        return static_cast<uint32_t>(beats / m_numerator) + 1;
    }

    double getTempo() const { return m_tempo; }
    void getTimeSignature(uint32_t& num, uint32_t& denom) const { 
        num = m_numerator; denom = m_denominator; 
    }

private:
    MusicalClock() : m_tempo(120.0), m_numerator(4), m_denominator(4) {}
    
    std::atomic<double> m_tempo;
    std::atomic<uint32_t> m_numerator;
    std::atomic<uint32_t> m_denominator;
};

} // namespace Aura::Core::Engine
