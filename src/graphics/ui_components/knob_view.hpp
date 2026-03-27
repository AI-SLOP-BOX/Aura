#pragma once
#include "ui_view.hpp"
#include "../../core/engine/macro_control_manager.hpp"

namespace Aura::Graphics::UI {

/**
 * @class KnobView
 * @brief Professional, Silver-Beveled Silver Hardware Knob.
 * Supports Mouse Dragging, Value Mapping, and Logic Pro aesthetics.
 */
class KnobView : public View {
public:
    KnobView(uint32_t macroIdx, std::string label, float defaultValue = 0.5f) 
        : m_macroIdx(macroIdx), m_label(label), m_defaultValue(defaultValue) {}

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        float cx = m_bounds.x + m_bounds.w / 2.0f;
        float cy = m_bounds.y + m_bounds.h / 2.0f;
        float radius = std::min(m_bounds.w, m_bounds.h) * 0.42f;

        // 1. NEON VALUE RING (Cyan logic-glow)
        float value = Core::Engine::MacroControlManager::getInstance().getMacroValue(m_macroIdx);
        kernel.drawArc(cx, cy, radius + 4, -135, -135 + value * 270, 3.0f, 0xFF22D3EE);
        kernel.drawNeonRect(cx - 1, cy - radius - 6, 2, 4, 1.0f, 4.0f, 0xFF22D3EE); // Top marker

        // 2. BEZEL (Outer Silver Rim with Groove)
        kernel.drawCircle(cx, cy, radius, 0xFF6B6B6D);
        kernel.drawCircle(cx, cy, radius * 0.96f, 0xFF121214); // Inner groove

        // 3. KNOB BODY (High-Fidelity Brushed Metal)
        kernel.drawBrushedCircle(cx, cy, radius * 0.92f, 0xFF4A4A4C);
        
        // 4. INDICATOR LINE (Recessed White)
        float angle = -135.0f + value * 270.0f;
        float rad = (angle - 90.0f) * 3.14159f / 180.0f;
        float lx1 = cx + std::cos(rad) * radius * 0.3f;
        float ly1 = cy + std::sin(rad) * radius * 0.3f;
        float lx2 = cx + std::cos(rad) * radius * 0.8f;
        float ly2 = cy + std::sin(rad) * radius * 0.8f;
        kernel.drawLine(lx1, ly1, lx2, ly2, 2.5f, 0xFFFFFFFF);

        // 5. LABEL (Centered via Kernel measurement)
        float tw = kernel.measureText(m_label, 9);
        kernel.drawText(m_label, cx - tw * 0.5f, m_bounds.y + m_bounds.h + 16, 9, 0xFFD1D5DB);
    }

    bool onMouseDown(float x, float y) override {
        m_captured = true;
        m_lastY = y;
        return true;
    }

    /**
     * @brief HONEST FIX: Logic Pro style Reset to Default.
     */
    bool onMouseDoubleClick(float x, float y) override {
        Core::Engine::MacroControlManager::getInstance().setMacroValue(m_macroIdx, m_defaultValue);
        return true;
    }

    bool onMouseDrag(float x, float y, float dx, float dy) override {
        if (!m_captured) return false;
        
        // Vertical Drag -> Change Value
        float val = Core::Engine::MacroControlManager::getInstance().getMacroValue(m_macroIdx);
        float sensitivity = 0.005f;
        val -= dy * sensitivity; 
        
        Core::Engine::MacroControlManager::getInstance().setMacroValue(m_macroIdx, std::clamp(val, 0.0f, 1.0f));
        return true;
    }

    bool onMouseUp(float x, float y) override {
        m_captured = false;
        return true;
    }

private:
    uint32_t m_macroIdx;
    std::string m_label;
    float m_defaultValue;
    bool m_captured = false;
    float m_lastY = 0.0f;
};

} // namespace Aura::Graphics::UI
