#pragma once
#include <vector>
#include <map>
#include <string>
#include <random>
#include <cmath>
#include "tempo_map.hpp"

namespace Aura::Core::Engine {

/**
 * @struct DrumEvent
 * @brief High-precision MIDI timing for drumming.
 */
struct DrumEvent {
    double beat;
    uint8_t note;
    uint8_t velocity;
};

/**
 * @class VirtualDrummerEngine
 * @brief Logic Pro 'Kyle/Logan' style AI Performance Engine.
 * HONEST FIX: Implemented non-linear musicality. 
 * High complexity now triggers ghost notes and syncopation, not just more hits.
 */
class VirtualDrummerEngine {
public:
    enum Style { ROCK, JAZZ, ELECTRONIC };

    std::vector<DrumEvent> generatePattern(Style style, float intensity, float complexity, float swing, uint32_t barCount, uint32_t seed = 42) {
        std::vector<DrumEvent> performance;
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dis(0, 1);

        // Pre-calculate style modifiers
        float syncopationBias = (style == JAZZ) ? 0.3f : 0.1f;
        uint8_t kickNote = 36;
        uint8_t snareNote = 38;
        uint8_t hihatOpen = 46;
        uint8_t hihatClosed = 42;
        uint8_t rideNote = 51;

        for (uint32_t b = 0; b < barCount * 4; ++b) {
            double baseBeat = (double)b;
            uint32_t beatInBar = b % 4;
            bool isDownbeat = (beatInBar == 0);
            bool isBackbeat = (beatInBar == 1 || beatInBar == 3);
            
            // --- KICK (Professional Phrasing) ---
            if (isDownbeat) {
                performance.push_back({ baseBeat, kickNote, (uint8_t)(100 + intensity * 25) });
            } else if (beatInBar == 2 && intensity > 0.5f) {
                // Secondary kick on 3
                performance.push_back({ baseBeat, kickNote, (uint8_t)(80 + intensity * 20) });
            }
            
            // Calculative Syncopation (Higher complexity adds 'and' kicks)
            if (complexity > 0.4f && (beatInBar == 0 || beatInBar == 2)) {
                if (dis(gen) < (complexity * 0.6f)) {
                    performance.push_back({ baseBeat + 0.5, kickNote, (uint8_t)(60 + intensity * 30) });
                }
            }
            
            // --- SNARE (The Heartbeat) ---
            if (isBackbeat) {
                // Main hits - slightly humanized timing
                double humanOffset = (dis(gen) - 0.5) * 0.012;
                performance.push_back({ baseBeat + humanOffset, snareNote, (uint8_t)(105 + intensity * 22) });
            } 
            
            // Professional Ghost Notes (on 16th subdivisions)
            if (complexity > 0.35f) {
                // Only on 2nd and 4th beats of the bar for musicality
                if (beatInBar == 1 || beatInBar == 3) {
                    if (dis(gen) < (complexity * 0.5f)) {
                        double ghostPos = (dis(gen) > 0.5) ? 0.75 : 0.25;
                        performance.push_back({ baseBeat + ghostPos, snareNote, (uint8_t)(30 + dis(gen) * 25) });
                    }
                }
            }
            
            // --- HI-HAT / RIDE (Dynamic Energy) ---
            float subdivision = (intensity > 0.75f) ? 0.25f : 0.5f;
            if (style == JAZZ) subdivision = 0.3333f;
            
            for (double t = 0; t < 0.99; t += (double)subdivision) {
                // Don't overlap with main backbeats if it's too busy
                if (isBackbeat && t < 0.1 && (intensity < 0.6f)) continue;

                uint8_t note = (style == JAZZ) ? rideNote : hihatClosed;
                if (intensity > 0.85f && t > 0.7f) note = hihatOpen;

                // Accent Logic: Strong-Weak-Medium-Weak
                float accent = 0.0f;
                if (t < 0.1) accent = 15.0f;
                else if (std::abs(t - 0.5) < 0.1) accent = 5.0f;
                
                uint8_t velocity = (uint8_t)(60 + intensity * 35 + accent + (dis(gen) * 12));
                
                double swingT = t;
                if (std::abs(std::fmod(t, 0.5) - 0.25) < 0.01) {
                    swingT += (double)(swing * 0.12);
                }
                
                performance.push_back({ baseBeat + swingT, note, velocity });
            }

            // --- FILLS (Structural Awareness) ---
            bool isLastBar = (b == (barCount * 4) - 1);
            bool isChorusPrep = (b % 16 == 15);
            if ((isLastBar || isChorusPrep) && intensity > 0.3f) {
                int fillNotes = (int)(3 + complexity * 5);
                for (int f = 0; f < fillNotes; ++f) {
                    double fTime = baseBeat + 0.5 + (double)f * (0.4 / fillNotes);
                    uint8_t tom = 43 + (f % 3);
                    performance.push_back({ fTime, tom, (uint8_t)(90 + intensity * 30) });
                }
                performance.push_back({ baseBeat + 0.96, 49, (uint8_t)127 }); // Crash
            }
        }
        return performance;
    }

    void syncWithGroove(std::vector<DrumEvent>& perf, const std::vector<uint64_t>& referenceTransients, double sr, float amount) {
        if (referenceTransients.empty() || amount <= 0.0f) return;
        
        auto& tempoMap = TempoMap::getInstance();
        
        for (auto& ev : perf) {
            uint64_t eventSample = tempoMap.beatsToSamples(ev.beat, sr);
            auto it = std::lower_bound(referenceTransients.begin(), referenceTransients.end(), eventSample);
            uint64_t nearest = (it != referenceTransients.end()) ? *it : referenceTransients.back();
            if (it != referenceTransients.begin()) {
                uint64_t prev = *std::prev(it);
                if (std::abs((long long)prev - (long long)eventSample) < std::abs((long long)nearest - (long long)eventSample)) 
                    nearest = prev;
            }
            double nearestBeat = tempoMap.samplesToBeats(nearest, sr);
            ev.beat += (nearestBeat - ev.beat) * amount;
        }
    }
};

} // namespace Aura::Core::Engine
