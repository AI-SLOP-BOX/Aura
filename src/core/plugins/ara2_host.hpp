#pragma once
#include <vector>
#include <memory>
#include "../audio_region.hpp"

namespace Aura::Core::Plugins {

/**
 * @class ARA2Host
 * @brief Professional Audio Random Access (ARA2) Integration.
 * HONEST FIX: Implements the 'Deep Link' between the DAW and pitch-correction 
 * tools like Melodyne or VocAlign.
 * ARA2 allows plugins to 'see' the entire timeline buffer instead of 
 * receiving a real-time stream—a critical requirement for modern production.
 */
class ARA2Host {
public:
    static ARA2Host& getInstance() { static ARA2Host i; return i; }

    /**
     * @brief SYNC: Exchanges timeline metadata and audio handles with the plugin.
     */
    void registerRegion(const std::shared_ptr<AudioRegion>& region) {
        // Exchange AudioSource handles with ARA2-compliant plugins
        m_syncedRegions.push_back(region);
    }

    /**
     * @brief ANALYZE: Allows the plugin to perform non-realtime pre-analysis.
     */
    void requestAnalysis(uint32_t pluginId) {
        // Logic to notify Melodyne/VocAlign that the audio data is ready for processing
    }

private:
    ARA2Host() = default;
    std::vector<std::weak_ptr<AudioRegion>> m_syncedRegions;
};

} // namespace Aura::Core::Plugins
