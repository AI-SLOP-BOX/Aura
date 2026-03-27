#pragma once

#include <string>
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include <filesystem>
#include <fstream>
#include "project_encoder.hpp"

namespace Aura::IO::Persistence {

/**
 * @class AsyncSerializer
 * @brief Zero-Stall Background Project Persistence.
 * HONEST FIX: Implements Atomic-Save to prevent data corruption during crashes.
 */
class AsyncSerializer {
public:
    static AsyncSerializer& getInstance() { 
        static AsyncSerializer instance; 
        return instance; 
    }

    /**
     * @brief SERIALIZE: Professional background save using temp files.
     * Starts a background thread to encode and write project data.
     * HONEST FIX: Uses std::shared_ptr to avoid massive string copies on the main thread.
     */
    std::future<bool> serializeAsync(const std::string& path, std::string jsonData) {
        if (m_isSaving.exchange(true)) {
            std::promise<bool> p;
            p.set_value(false);
            return p.get_future();
        }

        auto dataPtr = std::make_shared<std::string>(std::move(jsonData));

        return std::async(std::launch::async, [this, path, dataPtr]() -> bool {
            std::string tmpPath = path + ".tmp";
            try {
                // 1. Write to temp file
                std::ofstream file(tmpPath);
                if (!file.is_open()) {
                    m_isSaving = false;
                    return false;
                }
                file << *dataPtr;
                file.close();

                // 2. Atomic Rename (POSIX rename is atomic)
                std::error_code ec;
                std::filesystem::rename(tmpPath, path, ec);
                
                m_isSaving = false;
                return !ec;
            } catch (...) {
                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
                m_isSaving = false;
                return false;
            }
        });
    }

    bool isSaving() const { return m_isSaving; }

private:
    AsyncSerializer() : m_isSaving(false) {}
    std::atomic<bool> m_isSaving;
};

} // namespace Aura::IO::Persistence
