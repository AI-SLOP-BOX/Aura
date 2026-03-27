#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include "param_tree.hpp"
#include "timeline_system.hpp"

#include <nlohmann/json.hpp>

namespace Aura::Core::Engine {

class AutoSaveManager {
public:
    static AutoSaveManager& getInstance() { static AutoSaveManager i; return i; }

    struct ProjectSnapshot {
        nlohmann::json root;
    };

    void pushSnapshot(ProjectSnapshot&& snapshot) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_pendingSnapshot = std::move(snapshot);
        m_hasNewSnapshot = true;
    }

    void start(const std::string& projectPath) {
        m_projectPath = projectPath;
        if (m_running) return;
        m_running = true;
        m_worker = std::thread(&AutoSaveManager::workerLoop, this);
    }

    void stop() { m_running = false; if (m_worker.joinable()) m_worker.join(); }

private:
    void workerLoop() {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::minutes(2)); // Every 2 minutes for high-fidelity safety
            if (!m_running) break;

            ProjectSnapshot current;
            {
                std::lock_guard<std::mutex> lock(m_snapshotMutex);
                if (!m_hasNewSnapshot) continue;
                current = std::move(m_pendingSnapshot);
                m_hasNewSnapshot = false;
            }

            performSave(current);
        }
    }

    void performSave(const ProjectSnapshot& s) {
        std::string backupPath = m_projectPath + ".aura_backup";
        std::ofstream file(backupPath);
        if (!file.is_open()) return;
        file << s.root.dump(2);
        file.close();
        std::cout << "[AutoSave] Pro-grade JSON snapshot finalized." << std::endl;
    }


    std::atomic<bool> m_running{false};
    std::thread m_worker;
    std::string m_projectPath;
    
    std::mutex m_snapshotMutex;
    ProjectSnapshot m_pendingSnapshot;
    bool m_hasNewSnapshot = false;
};

} // namespace Aura::Core::Engine
