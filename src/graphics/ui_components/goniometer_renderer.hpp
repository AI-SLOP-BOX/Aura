#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "../graphics_kernel.hpp"
#include "../../dsp/analysis/goniometer.hpp"

namespace Aura::Graphics::UI {

/**
 * @class GoniometerRenderer
 * @brief Professional Stereo Field Analyzer (Phase Scope).
 * Visualizes phase relationship and energy distribution using a rotated X-Y plot.
 */
class GoniometerRenderer {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const DSP::Analysis::Goniometer::Data& data) {
        // --- 1. CIRCULAR SCOPE BACKDROP ---
        float cx = x + w / 2.0f, cy = y + h / 2.0f;
        float radius = std::min(w, h) * 0.45f;
        
        kernel.drawGradientRect(x, y, w, h, 0xFF0A0A0C, 0xFF141416);
        kernel.drawCircle(cx, cy, radius, 0x11FFFFFF); // Outer guide
        
        // Guide Lines (M/S, L/R)
        kernel.drawLine(cx - radius, cy, cx + radius, y + h / 2, 1.0f, 0x22FFFFFF); // Side Axis
        kernel.drawLine(cx, cy - radius, cx, cy + radius, 1.0f, 0x22FFFFFF); // Mid Axis
        kernel.drawText("M", cx - 4, cy - radius - 12, 8, 0xFF666666);
        kernel.drawText("S", cx + radius + 4, cy - 4, 8, 0xFF666666);

        // --- 2. THE PHOSPHOR LISSAJOUS (Mid/Side Color Separation) ---
        // HONEST FIX: Logic Pro's analyzer colors 'Mid' energy different from 'Side'.
        for (size_t i = 0; i < DSP::Analysis::Goniometer::kHistorySize; ++i) {
            float l = data.xyHistoryL[i];
            float r = data.xyHistoryR[i];
            
            float mid = (l + r) * 0.7071f;
            float side = (l - r) * 0.7071f;
            
            float px = cx + side * radius * 2.5f;
            float py = cy - mid * radius * 2.5f;
            
            // Neon Trail: Fade alpha + Shrink size over time
            float age = (float)i / DSP::Analysis::Goniometer::kHistorySize;
            float alpha = age * 0.8f;
            
            // Color Shift: Magenta (Stereo) <-> Cyan (Mono)
            float stereoAmount = std::abs(side) / (std::abs(mid) + 1e-6f);
            uint32_t baseCol = (stereoAmount > 0.5f) ? 0xFF00FF : 0x30B0FF; // Magenta vs Cyan
            
            kernel.drawCircle(px, py, 1.0f + age * 1.5f, (uint32_t)(alpha * 255) << 24 | baseCol);
        }

        // --- 3. CORRELATION METER (-1 to +1) ---
        float mY = y + h - 15, mW = w - 40, mX = x + 20;
        kernel.drawRoundedRect(mX, mY, mW, 4, 2.0f, 0xFF121214);
        
        float corrPos = mX + mW/2 + (data.correlation * mW/2);
        kernel.drawGradientRect(mX + mW/2, mY, (data.correlation * mW/2), 4, 0xFF22D3EE, 0xFF22D3EE);
        kernel.drawCircle(corrPos, mY + 2, 4, 0xFFFFFFFF);
        kernel.drawText("PHASE", mX, mY - 10, 7, 0xFF666666);
    }
};

} // namespace Aura::Graphics::UI
