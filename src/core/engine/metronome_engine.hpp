#pragma once

#include <cmath>
#include <atomic>
#include "grid_system.hpp"

namespace Aura::Core::Engine {

/**
 * @brief MetronomeEngine: High-precision timing click generator for recording.
 * Synchronizes with the GridSystem to provide downbeat and upbeat audible cues.
 */
class MetronomeEngine {
public:
    explicit MetronomeEngine(double sr) : m_sampleRate(sr) {}

    void process(float* l, float* r, size_t numFrames, uint64_t currentSample) {
        if (!m_isActive.load()) return;

        double samplesPerBeat = (60.0 / m_bpm) * m_sampleRate;
        for (size_t i = 0; i < numFrames; ++i) {
            uint64_t globalPos = currentSample + i;
            uint64_t beatPos = globalPos % static_cast<uint64_t>(samplesPerBeat);
            bool isDownbeat = (globalPos % static_cast<uint64_t>(samplesPerBeat * 4)) < 400;

            if (beatPos < 400) { // Click duration: ~9ms
                float freq = isDownbeat ? 1600.0f : 800.0f;
                float env = 1.0f - (static_cast<float>(beatPos) / 400.0f);
                float phase = (static_cast<float>(beatPos) * freq) / static_cast<float>(m_sampleRate);
                float sample = std::sin(phase * 6.283185f) * env * 0.2f;
                l[i] += sample; r[i] += sample;
            }
        }
    }

    void setActive(bool active) { m_isActive.store(active); }
    void setBPM(double bpm) { m_bpm = bpm; }

private:
    double m_sampleRate;
    std::atomic<bool> m_isActive{false};
    double m_bpm = 120.0;
    float m_phase = 0.0f;
};

} // namespace Aura::Core::Engine
