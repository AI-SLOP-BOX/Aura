#pragma once

#include <vector>
#include <string>
#include <map>

namespace Aura::Synthesis {

/**
 * @brief SamplerZone: A single mapping for a multi-sample instrument.
 */
struct SamplerZone {
    int minNote, maxNote;
    int minVel, maxVel;
    std::string samplePath;
    int roundRobinIdx = 0; // For cycling through variations
};

/**
 * @brief SamplerMap: Professional Virtual Instrument mapping logic.
 * Essential for High-End Sample Libraries (Piano/Strings/Drums).
 */
class SamplerMap {
public:
    void addZone(int minN, int maxN, int minV, int maxV, const std::string& path) {
        m_zones.push_back({ minN, maxN, minV, maxV, path, 0 });
    }

    /**
     * @brief FIND SAMPLE: Resolves the exact WAV path for a MIDI Note/Velocity.
     * Features Round Robin to prevent the 'Machine Gun Effect'.
     */
    std::string resolveSample(int note, int velocity) {
        std::vector<SamplerZone*> candidates;
        for (auto& z : m_zones) {
            if (note >= z.minNote && note <= z.maxNote && 
                velocity >= z.minVel && velocity <= z.maxVel) {
                candidates.push_back(&z);
            }
        }

        if (candidates.empty()) return "";

        // Round Robin: Cycle through candidates if multiple variations exist
        static uint32_t counter = 0;
        SamplerZone* selected = candidates[counter % candidates.size()];
        counter++;
        return selected->samplePath;
    }

private:
    std::vector<SamplerZone> m_zones;
};

} // namespace Aura::Synthesis
