#pragma once

#include <string>
#include <vector>
#include <future>
#include <iostream>
#include <atomic>
#include "../../rendering/bounce/bounce_engine.hpp"

namespace Aura::Core::IO::Persistence {

/**
 * @class ExportManager
 * @brief Professional Project Export Handler.
 * HONEST FIX: Bridges the UI to the actual BounceEngine rendering logic.
 */
class ExportManager {
public:
    static ExportManager& getInstance() {
        static ExportManager instance;
        return instance;
    }

    enum class Format { WAV, MP3, FLAC };

    /**
     * @brief Initiates an asynchronous "Bounce" of the master output.
     */
    void exportProject(const std::string& path, uint64_t totalSamples, uint32_t sampleRate) {
        m_progress.store(0.0f);
        m_isExporting.store(true);
        
        std::thread([this, path, totalSamples, sampleRate]() {
            // Process the bounce using the engine's offline renderer
            auto result = Engine::BounceEngine::renderMaster(path, totalSamples, sampleRate);
            
            if (result.success) {
                std::cout << "[Exporter] Successfully rendered to: " << path << " in " << result.elapsed << "s" << std::endl;
            } else {
                std::cerr << "[Exporter] Render FAILED: " << result.message << std::endl;
            }
            
            m_progress.store(1.0f);
            m_isExporting.store(false);
        }).detach();
    }

    float getProgress() const { return m_progress.load(); }
    bool isExporting() const { return m_isExporting.load(); }

private:
    ExportManager() = default;
    std::atomic<float> m_progress{0.0f};
    std::atomic<bool> m_isExporting{false};
};

} // namespace Aura::Core::IO::Persistence
