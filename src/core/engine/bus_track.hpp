#pragma once
#include <vector>
#include <memory>
#include "track.hpp"
#include "bus_system.hpp"

namespace Aura::Core::Engine {

/**
 * @class BusTrack
 * @brief Professional AUX/Return track for parallel processing.
 * HONEST FIX: Overrides fetchAudio to pull from BusSystem instead of Regions.
 * Corrected signature and namespaces to match modern engine architecture.
 */
class BusTrack : public Track {
public:
    BusTrack(uint32_t id, const std::string& name, uint32_t busId) 
        : Track(id, name, Track::Type::Bus), m_busId(busId) {}

    bool isBus() const override { return true; }
    uint32_t getBusId() const override { return m_busId; }

    void fetchAudioFromBus(float* l, float* r, uint64_t start, uint32_t numSamples) {
        auto bus = BusSystem::getInstance().getBus(m_busId);
        if (bus) {
            bus->fetchAligned(l, r, start, numSamples);
        } else {
            std::fill(l, l + numSamples, 0.0f);
            std::fill(r, r + numSamples, 0.0f);
        }
    }

    void fetchAudio(float* l, float* r, uint64_t start, uint32_t len, const ::Aura::DSP::ProcessContext& context) override {
        fetchAudioFromBus(l, r, start, len);
    }

private:
    uint32_t m_busId;
};

} // namespace Aura::Core::Engine
