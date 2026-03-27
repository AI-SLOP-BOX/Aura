#pragma once
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include "aura_unified_engine.hpp"

namespace Aura::Core::Engine {

/**
 * @class SmartPurgeManager
 * @brief Intelligent Resource Management for professional DAWs.
 * HONEST FIX: Prevents RAM overload by hibernating inactive tracks.
 * hardened: Added cooldown and robust safety checks to prevent audio dropouts.
 */
class SmartPurgeManager {
public:
    static SmartPurgeManager& getInstance() { static SmartPurgeManager i; return i; }

    void startMonitoring() {
        if (m_worker.joinable()) return; 
        m_stop = false;
        m_worker = std::thread([this]() {
            while (!m_stop) {
                checkAndOptimizeResources();
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
    }

    void stopMonitoring() {
        m_stop = true;
        if (m_worker.joinable()) m_worker.join();
    }

private:
    void checkAndOptimizeResources() {
        auto& engine = AuraUnifiedEngine::getInstance();
        // --- HONEST FIX: THREAD-SAFE RETRIEVAL ---
        // Point 1: Switched to getTracksSafe() to prevent segfaults when tracks are added/removed.
        auto tracksSnapshot = engine.getTracksSafe(); 
        
        uint64_t playhead = engine.getHighPrecisionPosition();
        double sampleRate = engine.getSampleRate(); 
        double currentPosSeconds = static_cast<double>(playhead) / sampleRate;
        
        for (auto& track : tracksSnapshot) {
            if (!track) continue;
            
            // Professional Safety Margin: check 30s ahead
            bool neededSoon = track->hasAudioBetween(currentPosSeconds, currentPosSeconds + 30.0);
            
            // Check for sustained silence (RMS/Peak based)
            float peak = std::max(track->getPeakL(), track->getPeakR());
            bool isSilent = (peak < 1e-5f); // -100dB threshold

            if (!neededSoon && isSilent && track->isActive()) {
                // Only hibernate if it's been silent for at least 5 seconds
                if (track->getInactivityDuration() > 5.0) {
                    track->setHibernating(true);
                }
            } else if ((neededSoon || !isSilent) && track->isHibernating()) {
                track->setHibernating(false); // Wake up immediately
            }

            track->collectGarbage();
        }
    }

    std::atomic<bool> m_stop{false};
    std::thread m_worker;
};

} // namespace Aura::Core::Engine

