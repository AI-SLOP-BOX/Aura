#pragma once

#include <vector>
#include <map>
#include <memory>

namespace Aura::Core::Engine {

/**
 * @brief SidechainSource: Metadata for a track or bus acting as a trigger.
 */
struct SidechainSource {
    uint32_t trackId;
    float level;
};

/**
 * @brief SidechainManager: Professional Global Signal Routing for dynamic effects.
 * Essential for modern EDM and Pop production (Ducking/Gating/Dynamic EQ).
 */
class SidechainManager {
public:
    static SidechainManager& getInstance() { static SidechainManager i; return i; }

    /**
     * @brief REGISTER: Designates a track as a potential sidechain trigger.
     */
    void registerSource(uint32_t trackId) {
        m_sources[trackId] = { trackId, 1.0f };
    }

    /**
     * @brief ROUTE: Connects a source to a specific destination plugin's sidechain input.
     */
    void route(uint32_t sourceTrackId, uint32_t destTrackId, uint32_t pluginIdx) {
        m_routing[destTrackId][pluginIdx] = sourceTrackId;
    }

    /**
     * @brief GET BUFFER: Retrieves the sidechain audio for a destination.
     */
    const float* getSidechainData(uint32_t destTrackId, uint32_t pluginIdx, uint32_t channel) {
        if (!m_routing[destTrackId].count(pluginIdx)) return nullptr;
        uint32_t srcId = m_routing[destTrackId][pluginIdx];
        
        // (Conceptual retrieval from TimelineSystem's track buffer)
        return nullptr;
    }

private:
    SidechainManager() = default;
    
    std::map<uint32_t, SidechainSource> m_sources;
    // DestTrackId -> PluginIdx -> SourceTrackId
    std::map<uint32_t, std::map<uint32_t, uint32_t>> m_routing;
};

} // namespace Aura::Core::Engine
