#pragma once

#include <cmath>
#include <cstdint>

namespace Aura::Core::Engine {

/**
 * @brief GridSystem: The musical scale of time.
 * Calculates snap points based on BPM, Time Signature, and rhythmic resolution.
 */
class GridSystem {
public:
    static GridSystem& getInstance() {
        static GridSystem instance;
        return instance;
    }

    void setTempo(double bpm) { m_bpm = bpm; }
    void setTimeSignature(int numerator, int denominator) {
        m_numerator = numerator;
        m_denominator = denominator;
    }

    /**
     * @brief Calculates the nearest snap position in samples.
     * @param inputSamples: The current raw sample position.
     * @param resolution: Rhythmic division (e.g., 4.0 for 1/4 notes, 16.0 for 1/16 notes).
     * @param sampleRate: Current engine sample rate.
     */
    double getSnappedSamples(double inputSamples, float resolution, double sampleRate) {
        double samplesPerBeat = (60.0 / m_bpm) * sampleRate;
        double snapInterval = (samplesPerBeat * 4.0) / resolution;
        
        double snapCount = std::round(inputSamples / snapInterval);
        return snapCount * snapInterval;
    }

    /**
     * @brief Converts beats to samples.
     */
    double beatsToSamples(double beats, double sampleRate) {
        return (beats * 60.0 / m_bpm) * sampleRate;
    }

private:
    GridSystem() = default;

    double m_bpm = 120.0;
    int m_numerator = 4;
    int m_denominator = 4;
};

} // namespace Aura::Core::Engine
