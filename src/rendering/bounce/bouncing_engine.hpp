#pragma once
#include <string>
#include <vector>
#include <future>
#include <iostream>
#include "timeline_system.hpp"
#include "../../io/wav_loader_utils.hpp"
#include "../status_queue.hpp"

namespace Aura::Core::Engine {

/**
 * @brief BouncingEngine: High-speed offline rendering for "Bounce-In-Place."
 */
class BouncingEngine {
public:
    static BouncingEngine& getInstance() {
        static BouncingEngine instance;
        return instance;
    }

    /**
     * @brief Renders a track's audio output into a local file.
     */
    std::future<bool> bounceInPlace(uint32_t trackId, const std::string& destinationPath) {
        return std::async(std::launch::async, [this, trackId, destinationPath]() {
            return this->performInternalRender(trackId, destinationPath);
        });
    }

private:
    BouncingEngine() = default;

    bool performInternalRender(uint32_t trackId, const std::string& path) {
        try {
            auto& timeline = TimelineSystem::getInstance();
            uint32_t numChannels = 2;
            uint32_t blockSize = 1024;
            double sampleRate = 44100.0;
            
            // For BIP, we render the entire occupied span of the track
            uint64_t totalSamples = 44100 * 60; // Mock: 60 seconds
            std::vector<std::vector<float>> exportBuffer(numChannels, std::vector<float>(totalSamples, 0.0f));

            ::Aura::Core::StatusQueue::getInstance().pushFromAudio(StatusQueue::Severity::Info, "Starting Bounce-In-Place...");

            for (uint64_t pos = 0; pos < totalSamples; pos += blockSize) {
                uint32_t frameCount = static_cast<uint32_t>(std::min<uint64_t>(blockSize, totalSamples - pos));
                
                // Temporary buffers for this block
                std::vector<float> l(frameCount), r(frameCount);
                
                // Render pull (This assumes TimelineSystem can pull specific track, 
                // but for now we pull master summing as a simplified BIP)
                timeline.render(l.data(), r.data(), frameCount);

                // Copy to export buffer
                std::copy(l.begin(), l.end(), exportBuffer[0].begin() + pos);
                std::copy(r.begin(), r.end(), exportBuffer[1].begin() + pos);
            }

            bool success = IO::WavSaver::save(path, exportBuffer, static_cast<uint32_t>(sampleRate));
            
            if (success) {
                ::Aura::Core::StatusQueue::getInstance().pushFromAudio(StatusQueue::Severity::Info, "Bounce-In-Place Completed: " + path);
            } else {
                ::Aura::Core::StatusQueue::getInstance().pushFromAudio(StatusQueue::Severity::Error, "Export Failed.");
            }

            return success;
        } catch (const std::exception& e) {
            std::cerr << "Bounce Exception: " << e.what() << std::endl;
            return false;
        }
    }
};

} // namespace Aura::Core::Engine
