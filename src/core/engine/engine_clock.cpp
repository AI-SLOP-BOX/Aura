#include "engine_clock.hpp"
#include <cstdio>
#include <cmath>

namespace Aura::Core::Engine {

EngineClock& EngineClock::getInstance() {
    static EngineClock instance;
    return instance;
}

void EngineClock::advance(uint64_t samples) {
    m_currentSample.fetch_add(samples);
}

void EngineClock::setPlayhead(uint64_t samples) {
    m_currentSample.store(samples);
}

double EngineClock::samplesToBeats(uint64_t samples) const {
    return TempoMap::getInstance().samplesToBeats(samples, m_sampleRate);
}

uint64_t EngineClock::beatsToSamples(double beats) const {
    return TempoMap::getInstance().beatsToSamples(beats, m_sampleRate);
}

MusicalTime EngineClock::getMusicalTime(uint64_t samples) const {
    double beats = samplesToBeats(samples);
    
    // Professional DAW logic (Logic Pro / Pro Tools style)
    // Assume 4/4 if timing map doesn't exist for simplicity here
    // But we use 480 PPQN (Pulses Per Quarter Note)
    int32_t totalTicks = static_cast<int32_t>(beats * 480.0);
    int32_t bar = (totalTicks / (4 * 480)) + 1;
    int32_t beat = ((totalTicks % (4 * 480)) / 480) + 1;
    int32_t sixteenth = ((totalTicks % 480) / 120) + 1;
    int32_t tick = (totalTicks % 120);

    MusicalTime mt;
    mt.bar = bar;
    mt.beat = beat;
    mt.sixteenth = sixteenth;
    mt.tick = tick;
    mt.totalBeats = beats;
    
    return mt;
}


uint64_t EngineClock::getCurrentSample() const {
    return m_currentSample.load();
}

} // namespace Aura::Core::Engine

