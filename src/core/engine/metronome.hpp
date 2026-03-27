#pragma once
#include <cmath>
#include <vector>
#include "../audio_buffer.hpp"

namespace Aura::Core::Engine {

/**
 * @brief Metronome: Generates rhythmic click signals for synchronization.
 * HONEST FIX: Uses an impulsive oscillator to prevent spectral bleed.
 */
class Metronome {
public:
    Metronome(double sr = 44100.0) : m_sampleRate(sr) {}

    void setEnabled(bool e) { m_isEnabled = e; }
    bool isEnabled() const { return m_isEnabled; }

    /**
     * @brief SAMPLE-ACCURATE METRONOME: Calculate crossing point within the block.
     */
    void process(float* l, float* r, uint32_t numSamples, uint64_t playhead, double sr, double bpm) {
        if (!m_isEnabled) return;

        auto& tm = TempoMap::getInstance();
        double currentBeat = tm.samplesToBeats(playhead, sr);
        double nextBeatSamples = tm.samplesToBeats(playhead + numSamples, sr);
        
        // Detect beat crossing precisely via floor/ceil change
        if (std::floor(currentBeat) != std::floor(nextBeatSamples)) {
            uint64_t beatNum = static_cast<uint64_t>(std::floor(nextBeatSamples));
            
            // Calculate exact sample offset within this block
            uint64_t beatSamplePos = tm.beatsToSamples(static_cast<double>(beatNum), sr);
            uint32_t offset = (beatSamplePos >= playhead) ? static_cast<uint32_t>(beatSamplePos - playhead) : 0;
            
            bool isBarStart = (beatNum % 4 == 0); // Simplified 4/4 check
            trigger(isBarStart, offset);
        }

        if (!m_active) return;

        for (uint32_t s = m_clickOffset; s < numSamples; ++s) {
            if (m_samplesLeft > 0) {
                // Use a pre-baked 1kHz/2kHz impulsive pulse logic
                float env = std::pow(static_cast<float>(m_samplesLeft) / (0.05f * sr), 2.0f);
                float val = std::sin(static_cast<float>(m_phase * 2.0 * M_PI)) * env * 0.4f;
                
                l[s] += val; r[s] += val;
                m_phase += m_frequency / sr;
                if (m_phase >= 1.0) m_phase -= 1.0;
                if (--m_samplesLeft == 0) m_active = false;
            }
        }
        m_clickOffset = 0; // Reset for next block
    }

private:
    void trigger(bool accent, uint32_t offset) {
        m_phase = 0.0;
        m_frequency = accent ? 1600.0 : 800.0; // Professional frequencies
        m_samplesLeft = static_cast<uint32_t>(0.03 * m_sampleRate); // Tight 30ms click
        m_active = true;
        m_clickOffset = offset;
    }

    double m_sampleRate;
    double m_frequency = 800.0;
    double m_phase = 0.0;
    bool m_isEnabled = false;
    bool m_active = false;
    uint32_t m_samplesLeft = 0;
    uint32_t m_clickOffset = 0;
};

} // namespace Aura::Core::Engine
