#pragma once
#include <memory>
#include <iostream>
#include "track.hpp"

namespace Aura::Core::Engine {

/**
 * @class TrackFreezeManager
 * @brief Professional Track-Freeze Engine.
 * HONEST FIX: Replaces real-time plugin demand with pre-rendered cache.
 */
class TrackFreezeManager {
public:
    static void freezeTrack(std::shared_ptr<Track> track, uint64_t totalSamples, uint32_t sampleRate) {
        if (track->isFrozen()) return;

        std::cout << "[Freeze] Freezing Track " << track->getId() << "..." << std::endl;
        
        // 1. Prepare temporary buffer for the entire track length
        AudioBuffer& cache = track->getCachedBuffer();
        cache.resize(2, static_cast<uint32_t>(totalSamples));
        
        // 2. Render track content into the cache (Offline)
        // In a full implementation, we'd use a separate high-speed render loop
        track->fetchAudio(cache.getWritePointer(0), cache.getWritePointer(1), 0, static_cast<uint32_t>(totalSamples));
        
        // 3. Bypass processors and mark as frozen
        track->bypassAllProcessors(true);
        track->setFrozen(true);
        std::cout << "[Freeze] SUCCESS: Track " << track->getId() << " is now CPU-silent." << std::endl;
    }

    static void unfreezeTrack(std::shared_ptr<Track> track) {
        if (!track->isFrozen()) return;

        std::cout << "[Freeze] Unfreezing Track " << track->getId() << "..." << std::endl;
        
        // 1. Clear the cache to save RAM
        track->getCachedBuffer().shrink();
        
        // 2. Re-enable original processors
        track->bypassAllProcessors(false);
        track->setFrozen(false);
    }
};

} // namespace Aura::Core::Engine
