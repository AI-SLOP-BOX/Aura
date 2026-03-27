#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include "tempo_map.hpp"
#include "../musical_time.hpp"

namespace Aura::Core::Engine {

class EngineClock {
public:
    static EngineClock& getInstance();
    
    void advance(uint64_t samples);
    void setPlayhead(uint64_t samples);
    
    double samplesToBeats(uint64_t samples) const;
    uint64_t beatsToSamples(double beats) const;
    
    MusicalTime getMusicalTime(uint64_t samples) const;
    uint64_t getCurrentSample() const;
    double getBPMAt(uint64_t samples) const { return TempoMap::getInstance().getBPMAt(samples); }
    
    void setSampleRate(double sr) { m_sampleRate = sr; }
    double getSampleRate() const { return m_sampleRate; }

private:
    EngineClock() : m_currentSample(0), m_sampleRate(44100.0) {}
    std::atomic<uint64_t> m_currentSample;
    double m_sampleRate;
};

} // namespace Aura::Core::Engine


