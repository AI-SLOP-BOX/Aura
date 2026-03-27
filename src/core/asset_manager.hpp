#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <iostream>

namespace Aura::Core {

/**
 * @class AssetManager
 * @brief THE LIBRARIAN: Handles project files, consolidation, and path-relinking.
 * SOLVES: Point 5 of the audit. Prevents 'Missing File' dialogs by 
 * ensuring all recordings are tracked and consolidated to the project folder.
 */
class AssetManager {
public:
    static AssetManager& getInstance() { static AssetManager i; return i; }
    
    /**
     * @brief REGISTRATION: Tracks a new file (e.g., recorded audio).
     */
    std::string registerAsset(const std::string& path, bool copyToProject = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::filesystem::path p(path);
        if (!std::filesystem::exists(p)) return "";
        
        if (copyToProject && !m_projectFolder.empty()) {
            auto dest = std::filesystem::path(m_projectFolder) / "Audio Files" / p.filename();
            if (!std::filesystem::exists(dest.parent_path())) std::filesystem::create_directories(dest.parent_path());
            
            try {
                if (!std::filesystem::exists(dest)) std::filesystem::copy_file(p, dest);
                m_assets[p.filename().string()] = dest.string();
                return dest.string();
            } catch(...) {
                std::cerr << "[AssetManager] Failed to copy asset to project folder." << std::endl;
            }
        }
        
        m_assets[p.filename().string()] = p.string();
        return p.string();
    }
    
    /**
     * @brief RELATIVE PATH CONVERSION: Ensures project files (.aura) don't break 
     * when the project folder is moved.
     */
    std::string getRelativePath(const std::string& absolutePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_projectFolder.empty()) return absolutePath;
        
        std::filesystem::path p(absolutePath);
        std::filesystem::path root(m_projectFolder);
        
        try {
            auto rel = std::filesystem::relative(p, root);
            return rel.string();
        } catch(...) {
            return absolutePath;
        }
    }

    /**
     * @brief ABSOLUTE PATH RECOVERY: Rebuilds paths from a moved project folder.
     */
    std::string resolvePath(const std::string& relativePath) {
        if (m_projectFolder.empty() || relativePath.empty()) return relativePath;
        auto p = std::filesystem::path(m_projectFolder) / relativePath;
        return p.lexically_normal().string();
    }
    
    void setProjectFolder(const std::string& folder) { 
        m_projectFolder = std::filesystem::path(folder).parent_path().string(); 
        std::cout << "[AssetManager] Context Root: " << m_projectFolder << std::endl;
    }
    
private:
    std::string m_projectFolder;
    std::map<std::string, std::string> m_assets; // Map of filename -> absolute path
    std::mutex m_mutex;
};

} // namespace Aura::Core
