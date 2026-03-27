#pragma once

#include <vector>
#include <string>
#include <algorithm>

namespace Aura::Core::Engine {

/**
 * @brief Marker: A project-wide milestone for navigation and structure.
 */
struct Marker {
    uint32_t id;
    std::string name;
    uint64_t samplePosition;
    uint32_t color; // Hex ARGB
};

/**
 * @brief MarkerSystem: Professional Project Navigation.
 * Standard for professional Arrangement workflows.
 */
class MarkerSystem {
public:
    static MarkerSystem& getInstance() { static MarkerSystem i; return i; }

    void addMarker(uint64_t pos, const std::string& name, uint32_t color = 0xFF555555) {
        Marker m = { static_cast<uint32_t>(m_markers.size()), name, pos, color };
        m_markers.push_back(std::move(m));
        sortMarkers();
    }

    /**
     * @brief Finds the marker immediately PREVIOUS to the given position.
     */
    const Marker* findPrevious(uint64_t pos) const {
        if (m_markers.empty()) return nullptr;
        auto it = std::lower_bound(m_markers.begin(), m_markers.end(), pos, [](const Marker& m, uint64_t p) {
            return m.samplePosition < p;
        });
        if (it == m_markers.begin()) return nullptr;
        return &(*std::prev(it));
    }

    /**
     * @brief Finds the marker immediately NEXT to the given position.
     */
    const Marker* findNext(uint64_t pos) const {
        auto it = std::upper_bound(m_markers.begin(), m_markers.end(), pos, [](uint64_t p, const Marker& m) {
            return p < m.samplePosition;
        });
        if (it == m_markers.end()) return nullptr;
        return &(*it);
    }

    const std::vector<Marker>& getMarkers() const { return m_markers; }

private:
    MarkerSystem() = default;
    
    void sortMarkers() {
        std::sort(m_markers.begin(), m_markers.end(), [](const Marker& a, const Marker& b) {
            return a.samplePosition < b.samplePosition;
        });
    }

    std::vector<Marker> m_markers;
};

} // namespace Aura::Core::Engine
