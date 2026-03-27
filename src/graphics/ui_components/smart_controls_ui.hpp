#pragma once
#include "ui_view.hpp"
#include "knob_view.hpp"

namespace Aura::Graphics::UI {

/**
 * @class SmartControlsUI
 * @brief Logic Pro style Smart Controls (Hardware Emulation).
 * HONEST REFACTOR: Inherits from Container to support interactivity.
 * Replaces the 'Hardcoded Picture' with a 'Live UI System'.
 */
class SmartControlsUI : public Container {
public:
    SmartControlsUI(const std::vector<std::string>& labels) {
        for (int i = 0; i < 8; ++i) {
            std::string label = (i < labels.size()) ? labels[i] : "PARAM " + std::to_string(i+1);
            auto knob = std::make_shared<KnobView>(i, label);
            addChild(knob);
            m_knobs.push_back(knob);
        }
    }

    void layout(float x, float y, float w, float h) {
        setBounds({x, y, w, h});
        
        float knobAreaW = w * 0.75f;
        float spacingX = knobAreaW / 4.0f;
        float spacingY = (h - 100) / 2.0f;

        for (int i = 0; i < 8; ++i) {
            float kx = x + 40 + (i % 4) * spacingX;
            float ky = y + 60 + (i / 4) * spacingY;
            m_knobs[i]->setBounds({kx - 25, ky - 25, 50, 50});
        }
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        if (!m_visible) return;
        
        // 1. EBONY & WOOD ENCLOSURE (Aesthetic Base)
        auto b = m_bounds;
        kernel.drawGradientRect(b.x, b.y, b.w, b.h, 0xFF2A2A2A, 0xFF1C1C1E);
        kernel.drawRoundedRect(b.x + 5, b.y + 5, b.w - 10, b.h - 10, 2.0f, 0xFF121214); // Inner panel
        
        // Wood Side Panels (Classic Logic Hardware)
        kernel.drawGradientRect(b.x, b.y, 10, b.h, 0xFF4A3728, 0xFF2A1C12);
        kernel.drawGradientRect(b.x + b.w - 10, b.y, 10, b.h, 0xFF4A3728, 0xFF2A1C12);

        // 2. RENDER CHILDREN (Knobs) - Container Handles Clipping
        Container::render(kernel);

        // 3. PHASE & LEVEL METERS (Logic Pro Style)
        float meterX = b.x + (b.w * 0.75f) + 20;
        float meterW = b.w - (meterX - b.x) - 30;
        
        // CORRELATION METER
        kernel.drawRoundedRect(meterX, b.y + 40, meterW, 14, 2.0f, 0xFF0A0A0C);
        kernel.drawLine(meterX + meterW/2, b.y + 40, meterX + meterW/2, b.y + 54, 1.0f, 0x33FFFFFF);
        kernel.drawCircle(meterX + meterW * 0.85f, b.y + 47, 4, 0xFF22D3EE); // Mock value
        kernel.drawText("CORRELATION", meterX + (meterW-50)/2, b.y + 30, 7, 0xFF888888);

        // MASTER OUTPUTS
        kernel.drawMeter(0, 0.45f, 0.42f, meterX, b.y + 70, meterW, b.h - 110);
        kernel.drawText("MASTER", meterX + (meterW-30)/2, b.y + b.h - 25, 8, 0xFF888888);
    }

private:
    std::vector<std::shared_ptr<KnobView>> m_knobs;
};

} // namespace Aura::Graphics::UI
