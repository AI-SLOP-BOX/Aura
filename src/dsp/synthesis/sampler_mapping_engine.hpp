#pragma once

#include <vector>
#include <string>
#include <algorithm>

namespace Aura::Core::DSP::Synthesis {

/**
 * @brief SamplerZone: Defines a mapping between MIDI input and an audio file.
 * Iconic Logic Pro EXS24-style multi-layer mapping.
 */
struct SamplerZone {
    uint8_t lowNote, highNote;
    uint8_t lowVel, highVel;
    std::string samplePath;
};

/**
 * @brief SamplerMappingEngine: Orchestrates multi-layered sampler instruments.
 * Essential for professional piano, drum, and orchestral instrument libraries.
 */
class SamplerMappingEngine {
public:
    static SamplerMappingEngine& getInstance() {
        static SamplerMappingEngine instance;
        return instance;
    }

    /**
     * @brief Finds the correct sample path for a given MIDI note and velocity.
     */
    std::string resolveSample(uint8_t note, uint8_t velocity) const {
        for (const auto& zone : m_zones) {
            if (note >= zone.lowNote && note <= zone.highNote &&
                velocity >= zone.lowVel && velocity <= zone.highVel) {
                return zone.samplePath;
            }
        }
        return ""; // Fallback
    }

    void addZone(const SamplerZone& zone) { m_zones.push_back(zone); }

private:
    SamplerMappingEngine() = default;
    std::vector<SamplerZone> m_zones;
};

} // namespace Aura::Core::DSP::Synthesis
