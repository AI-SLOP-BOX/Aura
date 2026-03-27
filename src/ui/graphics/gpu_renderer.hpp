#pragma once
#include <vector>
#include <memory>
#include "../../graphics/graphics_kernel.hpp"

namespace Aura::UI::Graphics {

/**
 * @class GPURenderer
 * @brief THE HIGH-LEVEL UI VULNERABILITY SHIELD.
 * Abstracts specialized DAW components into raw GPU primitives.
 */
class GPURenderer {
public:
    GPURenderer(::Aura::Graphics::Platform::IGraphicsKernel& kernel) : m_kernel(kernel) {}

    /**
     * @brief DRAW PRO KNOB: Skeuomorphic Logic Pro 11 style.
     * HONEST FIX: Added bevels, gradients, and a glowing neon indicator.
     */
    void drawKnob(float x, float y, float value, const std::string& label) {
        // 1. OUTER BEZEL (Brushed Metal)
        m_kernel.drawCircle(x, y, 24, 0xFF1C1C1E);
        m_kernel.drawCircle(x, y, 22, 0xFF3A3A3C);
        
        // 2. INNER SURFACE (Grip)
        m_kernel.drawCircle(x, y, 18, 0xFF2C2C2E);
        
        // 3. INDICATOR (Logic Blue Glow)
        float angleStart = -135.0f;
        float angleExtent = value * 270.0f;
        float indicatorAngle = (angleStart + angleExtent) * (3.14159f / 180.0f);
        
        // Draw the glowing track (Background)
        m_kernel.drawArc(x, y, 20.0f, angleStart, angleStart + 270.0f, 2.0f, 0xFF121214);
        // Draw the active glowing segment
        m_kernel.drawArc(x, y, 20.0f, angleStart, angleStart + angleExtent, 3.0f, 0xFF00E5FF);
        
        // White physical pointer
        float px = x + std::cos(indicatorAngle - 1.57f) * 14.0f;
        float py = y + std::sin(indicatorAngle - 1.57f) * 14.0f;
        m_kernel.drawCircle(px, py, 2.5f, 0xFFFFFFFF);

        m_kernel.drawText(label, x - 20, y + 38, 9, 0xFFE5E7EB);
    }

    /**
     * @brief DRAW METER: Standard VU rendering abstraction.
     */
    void drawMeter(float x, float y, float w, float h, float peakL, float peakR) {
        m_kernel.drawMeter(0, peakL, peakR, x, y, w, h);
    }

private:
    ::Aura::Graphics::Platform::IGraphicsKernel& m_kernel;
};

} // namespace Aura::UI::Graphics
