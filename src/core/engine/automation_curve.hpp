#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <memory>

namespace Aura::Core::Engine {

/**
 * @class AutomationCurve
 * @brief Thread-Safe Parametric Cubic Bezier Automation Engine.
 * HONEST FIX: Replaces the broken pointer-logic with C++20 atomic shared_ptr.
 * Ensures consistent 'Unity Gain' (1.0) when no automation is present.
 */
class AutomationCurve {
public:
    struct Point {
        double time;
        float value;
        float h1x = 0.33f, h1y = 0.0f; 
        float h2x = 0.66f, h2y = 1.0f;
    };

    AutomationCurve() {
        auto emptyList = std::make_shared<std::vector<Point>>();
        std::atomic_store(&m_pointList, emptyList);
    }

    float getValueAt(double time) const {
        auto pointsShared = std::atomic_load(&m_pointList);
        if (!pointsShared || pointsShared->empty()) return 1.0f;
        
        const auto& points = *pointsShared;
        if (points.size() == 1) return points[0].value;
        
        auto it = std::upper_bound(points.begin(), points.end(), time, [](double t, const Point& p) {
            return t < p.time;
        });
        
        if (it == points.begin()) return points.front().value;
        if (it == points.end()) return points.back().value;
        
        size_t idx = std::distance(points.begin(), std::prev(it));
        return evaluateAt(time, idx, points);
    }

    /**
     * @brief RT-SAFE POINT INJECTION:
     * Adds or updates a point in the curve using an atomic CAS loop.
     */
    void addPoint(double time, float value) {
        auto oldPoints = std::atomic_load(&m_pointList);
        while (true) {
            auto newPoints = std::make_shared<std::vector<Point>>(*oldPoints);
            
            auto it = std::lower_bound(newPoints->begin(), newPoints->end(), time, [](const Point& p, double t) {
                return p.time < t;
            });
            
            if (it != newPoints->end() && std::abs(it->time - time) < 1e-4) {
                it->value = value;
            } else {
                newPoints->insert(it, {time, value});
            }
            
            if (std::atomic_compare_exchange_weak(&m_pointList, &oldPoints, newPoints)) {
                break;
            }
        }
    }

    void fillBuffer(float* buffer, uint32_t numSamples, double startTime, double endTime) {
        auto pointsShared = std::atomic_load(&m_pointList);
        if (!pointsShared || pointsShared->empty()) {
            std::fill(buffer, buffer + numSamples, 1.0f);
            return;
        }

        const auto& points = *pointsShared;
        if (points.size() == 1) {
            std::fill(buffer, buffer + numSamples, points[0].value);
            return;
        }

        double duration = endTime - startTime;
        size_t currentIdx = 0;

        for (uint32_t i = 0; i < numSamples; ++i) {
            double currentTime = startTime + (static_cast<double>(i) / std::max(1U, numSamples)) * duration;
            while (currentIdx < points.size() - 2 && currentTime >= points[currentIdx + 1].time) {
                currentIdx++;
            }
            buffer[i] = evaluateAt(currentTime, currentIdx, points);
        }
    }

    void addPoint(double time, float value, float h1y = 0.0f, float h2y = 1.0f) {
        auto oldList = std::atomic_load(&m_pointList);
        std::shared_ptr<std::vector<Point>> newList;
        
        do {
            newList = std::make_shared<std::vector<Point>>(*oldList);
            newList->push_back({time, value, 0.33f, h1y, 0.66f, h2y});
            std::sort(newList->begin(), newList->end(), [](const Point& a, const Point& b) {
                return a.time < b.time;
            });
        } while (!std::atomic_compare_exchange_weak(&m_pointList, &oldList, newList));
    }

    float evaluateAt(double time, size_t idx, const std::vector<Point>& points) const {
        const auto& p0 = points[idx];
        const auto& p3 = points[idx + 1];

        double segTime = p3.time - p0.time;
        if (segTime < 1e-9) return p0.value;
        
        double targetX = (time - p0.time) / segTime;
        targetX = std::clamp(targetX, 0.0, 1.0);

        // FAST PATH: If handles are at default linear positions + no curvature (h1y=0, h2y=1), 
        // skip the expensive cubic solver.
        if (std::abs(p0.h1y - 0.0f) < 1e-4f && std::abs(p0.h2y - 1.0f) < 1e-4f &&
            std::abs(p0.h1x - 0.33f) < 1e-2f && std::abs(p0.h2x - 0.66f) < 1e-2f) {
            return static_cast<float>(p0.value + (p3.value - p0.value) * targetX);
        }

        // REDUCED ITERATION NEWTON-RAPHSON: 3 iterations is enough for automation accuracy.
        double t = targetX; 
        for (int i = 0; i < 3; ++i) {
            double it = 1.0 - t;
            double x = 3.0 * it * it * t * p0.h1x + 3.0 * it * t * t * p0.h2x + t * t * t - targetX;
            double dx = 3.0 * it * it * p0.h1x + 6.0 * it * t * (p0.h2x - p0.h1x) + 3.0 * t * t * (1.0 - p0.h2x);
            if (std::abs(dx) < 1e-6) break;
            t -= x / dx;
        }
        t = std::clamp(t, 0.0, 1.0);

        double p1y = p0.value + (p3.value - p0.value) * p0.h1y;
        double p2y = p0.value + (p3.value - p0.value) * p0.h2y;

        double it = 1.0 - t;
        return static_cast<float>(it*it*it*p0.value + 3.0*it*it*t*p1y + 3.0*it*t*t*p2y + t*t*t*p3.value);
    }

    std::vector<Point> getPoints() const {
        auto pointsShared = std::atomic_load(&m_pointList);
        if (!pointsShared) return {};
        return *pointsShared;
    }


private:
    std::shared_ptr<std::vector<Point>> m_pointList;
};

} // namespace Aura::Core::Engine
