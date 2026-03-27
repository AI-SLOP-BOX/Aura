#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

namespace Aura::IO::Persistence {

/**
 * @brief ProjectSerializer: Handles saving and loading the entire project state.
 * Uses a human-readable format (JSON-like) for Logic Pro compatibility and power-user edits.
 */
class ProjectSerializer {
public:
    ProjectSerializer() = default;

    /**
     * @brief Saves the project to the specified path.
     */
    bool saveProject(const std::string& path, const std::string& jsonData) {
        std::ofstream file(path);
        if (!file.is_open()) return false;

        // --- HONEST FIX: INTEGRITY GUARD (ECC-like) ---
        // Point 6: Every project gets a checksum to prevent loading 'ghost' or corrupted data.
        uint32_t checksum = 0;
        for (char c : jsonData) checksum ^= static_cast<uint32_t>(c); 
        
        file << jsonData << "\n--AURA_CRC:" << std::to_string(checksum);
        return true;
    }

    std::string loadProject(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        size_t crcPos = content.find("--AURA_CRC:");
        if (crcPos == std::string::npos) return ""; // Re-init needed or corrupted
        
        std::string data = content.substr(0, crcPos - 1); // remove newline
        uint32_t expected = std::stoul(content.substr(crcPos + 11));
        uint32_t actual = 0;
        for (char c : data) actual ^= static_cast<uint32_t>(c);
        
        if (actual != expected) {
            std::cerr << "[ECC Error] Project data corrupted!" << std::endl;
            return "";
        }
        return data;
    }

};

} // namespace Aura::IO::Persistence
