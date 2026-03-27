#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class LoudnessRenderer
 * @brief EBU R128 Compliant LUFS Metering UI.
 * High-precision numerical display + color-coded safety bars.
 */
class LoudnessRenderer {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, float lufs) {
        // --- 1. PREMIUM EBONY GLASS ---
        kernel.drawGradientRect(x, y, w, h, 0xFF0D0D0F, 0xFF141416);
        kernel.drawRoundedRect(x, y, w, h, 6.0f, 0x11FFFFFF); // Subtle border

        // --- 2. LUFS NUMERICAL (Logic 11 Digital Font Style) ---
        char lufsStr[16];
        snprintf(lufsStr, 16, lufs < -70.0f ? "-INF" : "%.1f", lufs);
        uint32_t textCol = (lufs > -12.0f) ? 0xFFFF3B30 : (lufs > -14.0 ? 0xFFFFD60A : 0xFF30B0FF);
        kernel.drawText(lufsStr, x + 20, y + 35, 24, textCol);
        kernel.drawText("LUFS-I", x + w - 50, y + 30, 8, 0xFF666666);

        // --- 3. SEGMENTED LED BAR ---
        float barX = x + 15, barY = y + 55, barW = w - 30, barH = 12;
        kernel.drawRoundedRect(barX, barY, barW, barH, 2.0f, 0xFF121214);
        
        float norm = std::clamp((lufs + 40.0f) / 40.0f, 0.0f, 1.0f);
        uint32_t numSegments = 40;
        float segW = (barW / numSegments) - 1.0f;
        
        for (uint32_t i = 0; i < numSegments; ++i) {
            float progress = (float)i / numSegments;
            if (progress > norm) break;
            
            // LED Colors: Green -> Yellow -> Orange -> Red
            uint32_t ledCol = (progress > 0.85f) ? 0xFFFF3B30 : (progress > 0.7f ? 0xFFFFD60A : 0xFF34C759);
            kernel.drawRect(barX + i * (segW + 1), barY + 1, segW, barH - 2, ledCol);
        }

        // --- 4. GLASS REFLECTION (Logic Pro Aesthetic) ---
        kernel.drawGradientRect(x, y, w, h / 2.0f, 0x1AFFFFFF, 0x00FFFFFF);
    }
};

} // namespace Aura::Graphics::UI
