#pragma once

#include <vector>
#include <map>
#include <mutex>
#include <memory>

namespace Aura::Core::DSP::Mixing {

/**
 * @brief SidechainBus: A high-performance auxiliary audio buffer for routing control signals.
 */
struct SidechainBus {
    std::vector<float> buffer;
    uint32_t sourceTrackId;
};

/**
 * @brief SidechainManager: Global router for track interaction and ducking triggers.
 * Enables professional-grade "Kick vs Bass" ducking and cross-track modulation.
 */
class SidechainManager {
public:
    static SidechainManager& getInstance() {
        static SidechainManager instance;
        return instance;
    }

    /**
     * @brief Writes the current track output into a sidechain bus for others to read.
     */
    void writeSource(uint32_t trackId, const float* data, size_t numFrames) {
        std::lock_guard<std::mutex> lock(m_busMutex);
        auto& bus = m_buses[trackId];
        if (bus.buffer.size() < numFrames) bus.buffer.resize(numFrames);
        
        std::copy(data, data + numFrames, bus.buffer.begin());
        bus.sourceTrackId = trackId;
    }

    /**
     * @brief Reads a specific source bus for an effect. (Real-time safe READ).
     */
    const float* readSource(uint32_t trackId) {
        auto it = m_buses.find(trackId);
        return (it != m_buses.end()) ? it->second.buffer.data() : nullptr;
    }

private:
    SidechainManager() = default;

    std::map<uint32_t, SidechainBus> m_buses;
    std::mutex m_busMutex;
};

} // namespace Aura::Core::DSP::Mixing
