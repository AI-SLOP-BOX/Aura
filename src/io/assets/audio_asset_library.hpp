#pragma once

#include <string>
#include <vector>
#include <map>

namespace Aura::IO::Assets {

/**
 * @brief AssetPatch: A high-quality Logic Pro-style instrument preset.
 */
struct AssetPatch {
    std::string name;
    std::string category;
    std::string filePath;
};

/**
 * @brief AudioAssetLibrary: Central manifest for Factory Content.
 * Iconic Logic Pro feature that allows users to find high-end sounds instantly.
 */
class AudioAssetLibrary {
public:
    static AudioAssetLibrary& getInstance() {
        static AudioAssetLibrary instance;
        return instance;
    }

    /**
     * @brief Adds a new patch to the list.
     */
    void registerPatch(const std::string& name, const std::string& cat, const std::string& path) {
        m_patches.push_back({name, cat, path});
    }

    /**
     * @brief Filters patches by category (e.g., "Drum", "Piano").
     */
    std::vector<AssetPatch> findByCategory(const std::string& cat) const {
        std::vector<AssetPatch> results;
        for (const auto& p : m_patches) {
            if (p.category == cat) results.push_back(p);
        }
        return results;
    }

private:
    AudioAssetLibrary() = default;

    std::vector<AssetPatch> m_patches;
};

} // namespace Aura::IO::Assets
