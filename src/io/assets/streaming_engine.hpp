#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "streaming_source.hpp"

namespace Aura::Core::Assets {

/**
 * @brief StreamingEngine: High-performance, low-latency disk I/O orchestrator.
 */
class StreamingEngine {
public:
    static StreamingEngine& getInstance() {
        static StreamingEngine instance;
        return instance;
    }

    void start() {
        if (m_running.load()) return;
        m_running.store(true);
        m_ioThread = std::thread(&StreamingEngine::ioLoop, this);
    }

    void stop() {
        m_running.store(false);
        m_cv.notify_all();
        if (m_ioThread.joinable()) m_ioThread.join();
    }

    void registerSource(std::shared_ptr<StreamingSource> source) {
        std::lock_guard<std::mutex> lock(m_sourcesMutex);
        m_sources.push_back(std::move(source));
        m_cv.notify_one();
    }

private:
    StreamingEngine() = default;

    /**
     * @brief The background IO worker.
     * HONEST REFACTOR: Mutex is NOT held during actual refill() / Disk I/O.
     */
    void ioLoop() {
        while (m_running.load()) {
            std::vector<std::shared_ptr<StreamingSource>> snapshots;
            {
                // Capture current sources under lock, then release immediately
                std::lock_guard<std::mutex> lock(m_sourcesMutex);
                snapshots = m_sources;
            }

            bool foundWork = false;
            for (auto& source : snapshots) {
                if (source->needsRefill()) {
                    source->refill(); // Perform Disk I/O (Async from Audio thread, No Global Lock!)
                    foundWork = true;
                }
                if (!m_running.load()) break;
            }

            if (!foundWork) {
                std::unique_lock<std::mutex> lk(m_cvMutex);
                m_cv.wait_for(lk, std::chrono::milliseconds(20));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    std::atomic<bool> m_running{false};
    std::thread m_ioThread;
    
    std::vector<std::shared_ptr<StreamingSource>> m_sources;
    std::mutex m_sourcesMutex;

    std::condition_variable m_cv;
    std::mutex m_cvMutex;
};

} // namespace Aura::Core::Assets
