#pragma once

#include <string>
#include <map>

namespace Aura::Core::Engine {

/**
 * @brief GlobalKeyEvent: A key change at a specific timeline position.
 */
struct GlobalKeyEvent {
    uint64_t samplePosition;
    std::string keyName; // e.g., "Cm", "Fmajor"
};

/**
 * @brief GlobalTrackSystem: Central metadata for song structure.
 * Logic Pro-style "Global Tracks" covering Keys, Markers, and Signatures.
 */
class GlobalTrackSystem {
public:
    static GlobalTrackSystem& getInstance() {
        static GlobalTrackSystem instance;
        return instance;
    }

    /**
     * @brief Sets the musical key at a given timeline position.
     */
    void addKeyChange(uint64_t pos, const std::string& keyName) {
        m_keyMap[pos] = keyName;
    }

    /**
     * @brief Resolves the current key at any point in the song.
     */
    std::string resolveKeyAt(uint64_t pos) const {
        auto it = m_keyMap.lower_bound(pos);
        if (it != m_keyMap.begin()) return std::prev(it)->second;
        return m_keyMap.empty() ? "Cmajor" : m_keyMap.begin()->second;
    }

private:
    GlobalTrackSystem() = default;

    // Sample Position -> Key Name
    std::map<uint64_t, std::string> m_keyMap;
};

} // namespace Aura::Core::Engine
