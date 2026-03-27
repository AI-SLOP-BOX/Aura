#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "../graphics_kernel.hpp"
#include "../../core/engine/automation_curve.hpp"
#include "../../ui/main/view_transformer.hpp"

namespace Aura::Graphics::UI {

/**
 * @class AutomationRenderer
 * @brief High-performance Bezier curve visualization for track automation.
 */
class AutomationRenderer {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, 
                const std::vector<Core::Engine::AutomationCurve::Point>& points, 
                double startBeats, double endBeats, uint32_t color) {
        
        if (points.empty()) return;

        auto& vt = ::Aura::UI::Main::ViewTransformer::getInstance();
        float beatW = w / (float)(endBeats - startBeats);

        // --- 1. THE BEZIER RENDER (Sub-pixel precise) ---
        static Core::Engine::AutomationCurve math;
        std::vector<::Aura::Graphics::Vertex> curvePoints;
        curvePoints.reserve(w / 4);

        size_t currentIdx = 0;
        for (int px = 0; px < (int)w; px += 2) {
            double currentBeat = startBeats + (px / (double)w) * (endBeats - startBeats);
            while (currentIdx < points.size() - 2 && currentBeat >= points[currentIdx+1].time) {
                currentIdx++;
            }
            float val = math.evaluateAt(currentBeat, currentIdx, points);
            float py = y + h - (val * h);
            curvePoints.push_back({x + px, py, color});
        }

        kernel.drawVertexPath(curvePoints.data(), curvePoints.size(), 1.8f);

        // --- 2. CONTROL POINTS (Logic Pro Circles) ---
        for (const auto& p : points) {
            if (p.time < startBeats || p.time > endBeats) continue;
            float px = x + (float)(p.time - startBeats) * beatW;
            float py = y + h - (p.value * h);
            kernel.drawCircle(px, py, 4.0f, 0xFFFFFFFF);
            kernel.drawCircle(px, py, 3.0f, color);
        }
    }

    int hitTest(float mx, float my, float x, float y, float w, float h, 
                const std::vector<Core::Engine::AutomationCurve::Point>& points, 
                double startBeats, double endBeats) {
        float beatW = w / (float)(endBeats - startBeats);
        for (int i = 0; i < (int)points.size(); ++i) {
            float px = x + (float)(points[i].time - startBeats) * beatW;
            float py = y + h - (points[i].value * h);
            float dist = std::sqrt((mx - px) * (mx - px) + (my - py) * (my - py));
            if (dist < 10.0f) return i;
        }
        return -1;
    }

    /**
     * @brief HIT TEST FOR SEGMENTS (Logic Pro Curve Tool)
     * Detects if the mouse is hovering over the curve between two points.
     */
    int hitTestSegment(float mx, float my, float x, float y, float w, float h, 
                       const std::vector<Core::Engine::AutomationCurve::Point>& points, 
                       double startBeats, double endBeats) {
        if (points.size() < 2) return -1;
        
        static Core::Engine::AutomationCurve math;
        
        // Brute force check every few pixels for precision (Logic matches this)
        for (int px = 0; px < (int)w; px += 4) {
             double currentBeat = startBeats + (px / (double)w) * (endBeats - startBeats);
             
             // Find current segment index
             auto it = std::upper_bound(points.begin(), points.end(), currentBeat, [](double t, const auto& p) {
                 return t < p.time;
             });
             if (it == points.begin() || it == points.end()) continue;
             size_t idx = std::distance(points.begin(), std::prev(it));

             float val = math.evaluateAt(currentBeat, idx, points);
             float py = y + h - (val * h);
             float dist = std::sqrt((mx - (x + px)) * (mx - (x + px)) + (my - py) * (my - py));
             
             if (dist < 8.0f) return (int)idx;
        }
        return -1;
    }

private:
    float evaluateForVis(double time, const std::vector<Core::Engine::AutomationCurve::Point>& points) {
        if (points.empty()) return 0.5f;
        if (time <= points.front().time) return points.front().value;
        if (time >= points.back().time) return points.back().value;

        // Binary search for the correct segment
        auto it = std::upper_bound(points.begin(), points.end(), time, [](double t, const auto& p) {
            return t < p.time;
        });
        
        size_t idx = std::distance(points.begin(), it) - 1;
        const auto& p0 = points[idx];
        const auto& p3 = points[idx + 1];

        // Linear interpolation for simple Vis (or implement full Bezier if needed)
        float t = (time - p0.time) / (p3.time - p0.time + 1e-9);
        return p0.value + t * (p3.value - p0.value);
    }
};

} // namespace Aura::Graphics::UI
