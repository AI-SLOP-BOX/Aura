#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

namespace Aura::IO::Persistence {

/**
 * @brief ProjectCollector: Ensures project portability.
 * Scans project for all external assets and copies them into a unified folder.
 */
class ProjectCollector {
public:
    static ProjectCollector& getInstance() {
        static ProjectCollector instance;
        return instance;
    }

    /**
     * @brief "Collect All and Save" - Aggregates all used files.
     */
    bool collect(const std::string& projectDir, const std::vector<std::string>& assetPaths) {
        std::filesystem::path destDir = std::filesystem::path(projectDir) / "Assets";
        std::filesystem::create_directories(destDir);

        for (const auto& originalPath : assetPaths) {
            try {
                std::filesystem::path src(originalPath);
                std::filesystem::path dest = destDir / src.filename();
                
                if (std::filesystem::exists(src)) {
                    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
                    std::cout << "[Collector] Collected: " << src.filename() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Collector Alert] Error copying: " << originalPath << " (" << e.what() << ")" << std::endl;
                return false;
            }
        }
        return true;
    }

private:
    ProjectCollector() = default;
};

} // namespace Aura::IO::Persistence
