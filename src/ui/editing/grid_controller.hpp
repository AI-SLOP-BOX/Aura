#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace Aura::UI::Editing {

/**
 * @class GridController
 * @brief Logic Pro-style 'Smart Grid' and 'Snap Magnetism' Engine.
 * HONEST FIX: Replaces rigid grid division with a dynamic 'Stress-Free' 
 * snapping system that scales with zoom and allows for intuitive 'Magnetic' 
 * behavior.
 */
class GridController {
public:
    enum class GridMode { SMART, BAR, BEAT, DIVIDE, TICKS };

    /**
     * @brief THE SMART GRID: Determines the visual and snap resolution 
     * based on the current horizontal zoom level.
     */
    double getGridDivision(double zoomLevel) const {
        // As zoomLevel increases (pixels per beat), the division gets finer.
        if (zoomLevel < 10.0) return 4.0;      // 1 Bar
        if (zoomLevel < 40.0) return 1.0;      // 1 Beat (1/4)
        if (zoomLevel < 150.0) return 0.5;     // 1/8
        if (zoomLevel < 600.0) return 0.25;    // 1/16
        return 0.125;                          // 1/32
    }

    /**
     * @brief MAGNETIC SNAP: Snaps the raw position to the nearest grid 
     * line, but ONLY if it's within the 'Magnetic Strength' range.
     * @param magnetism 0.0 (No snap) to 1.0 (Full snap from anywhere)
     */
    double getSnappedPosition(double rawPosition, double zoomLevel, float magnetism) const {
        double division = getGridDivision(zoomLevel);
        double nearestGrid = std::round(rawPosition / division) * division;
        double distance = std::abs(rawPosition - nearestGrid);

        // Calculate magnetic threshold in 'Beats' based on magnetism 0.0-1.0
        // (At 1.0, it snaps up to 40% of the division away)
        double threshold = (division * 0.4) * magnetism;
        
        if (distance <= threshold) {
            return nearestGrid;
        }
        return rawPosition; // Stay 'free' if outside magnetic pull
    }

    enum class GridType { BAR, BEAT, DIVIDE };

    /**
     * @brief THE HIERARCHICAL GRID: Prevents 'Grid Overload' stress.
     * Identifies exactly which grid line a coordinate belongs to.
     */
    GridType getGridType(double beatPos) const {
        if (std::fmod(beatPos, 4.0) < 0.001) return GridType::BAR;    // Every 4 beats (Standard 4/4)
        if (std::fmod(beatPos, 1.0) < 0.001) return GridType::BEAT;   // Every beat
        return GridType::DIVIDE;                                      // Sub-divisions (1/16, etc.)
    }

    /**
     * @brief RENDER LOGIC: Returns all visible lines with their hierarchy.
     */
    struct VisualGridLine { double pos; GridType type; };
    std::vector<VisualGridLine> getHierarchicalGrid(double start, double end, double zoomLevel) const {
        std::vector<VisualGridLine> res;
        double div = getGridDivision(zoomLevel);
        
        for (double t = std::ceil(start / div) * div; t <= end; t += div) {
            res.push_back({ t, getGridType(t) });
        }
        return res;
    }
};

} // namespace Aura::UI::Editing
