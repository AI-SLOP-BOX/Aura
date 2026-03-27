#pragma once

#include <string>
#include <vector>
#include <map>

namespace Aura::Core::Engine {

/**
 * @brief Marker: Timeline anchor with a name and color metadata.
 */
struct Marker {
    uint64_t samplePosition;
    std::string name;
    uint32_t colorHex = 0x888888;
};

/**
 * @brief ProjectMarkerSystem: High-performance timeline navigation anchors.
 * Logic Pro-style Markers for Verse, Chorus, and arrangement labels.
 */
class ProjectMarkerSystem {
public:
    static ProjectMarkerSystem& getInstance() {
        static ProjectMarkerSystem instance;
        return instance;
    }

    /**
     * @brief Adds a marker to a specific sample position.
     */
    void addMarker(uint64_t pos, const std::string& name) {
        m_markers[pos] = {pos, name, 0x00FF00};
    }

    /**
     * @brief Finds the closest marker before a position (for navigation).
     */
    const Marker* findPreviousMarker(uint64_t pos) const {
        auto it = m_markers.lower_bound(pos);
        if (it != m_markers.begin()) return &std::prev(it)->second;
        return nullptr;
    }

    const std::map<uint64_t, Marker>& getAllMarkers() const { return m_markers; }

private:
    ProjectMarkerSystem() = default;

    // Sample Position -> Marker
    std::map<uint64_t, Marker> m_markers;
};

} // namespace Aura::Core::Engine
