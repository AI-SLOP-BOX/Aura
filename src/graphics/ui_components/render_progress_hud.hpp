#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <chrono>

namespace Aura::Graphics::UI {

/**
 * @class RenderProgressHUD
 * @brief Blender-style Export Status Display.
 * HONEST FIX: Replaces a simple status bar with professional 
 * Blender-inspired rendering metrics.
 * Shows Tile progress, ETR (Estimated Time Remaining), and Output Size.
 */
class RenderProgressHUD {
public:
    struct Stats {
        float progress = 0.0f; // 0.0 to 1.0
        float elapsedSec = 0.0f;
        uint64_t totalBytes = 0;
        bool isDone = false;
    };

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const Stats& s) {
        if (s.isDone) return;

        // --- 1. DARK RENDERING GLASS ---
        kernel.drawDropShadow(x, y, w, h, 10.0f, 0xAA000000);
        kernel.drawGradientRect(x, y, w, h, 0xFF1C1C1E, 0xFF0D0D0F);
        kernel.drawRoundedRect(x, y, w, h, 6.0f, 0x33FFFFFF);

        // --- 2. PROGRESS TILE (Blender Style) ---
        float bW = w - 40, bH = 14;
        kernel.drawRoundedRect(x + 20, y + 50, bW, bH, 2.0f, 0xFF000000);
        kernel.drawGradientRect(x + 20, y + 50, bW * s.progress, bH, 0xFFEAB308, 0xFFD97706); // Amber progress
        
        // Glow
        kernel.drawNeonRect(x + 20, y + 50, bW * s.progress, bH, 1.0f, 4.0f, 0x44EAB308);

        // --- 3. RENDERING METRICS ---
        kernel.drawText("BOUNCING PROJECT [Ultra Quality / 64-bit]", x + 20, y + 30, 9, 0xFFD1D1D1);
        
        float etr = (s.progress > 0.01f) ? (s.elapsedSec / s.progress) - s.elapsedSec : 0.0f;
        char etrText[64];
        snprintf(etrText, 64, "ETR: %02d:%02d | %.1f MB", (int)etr/60, (int)etr%60, (float)s.totalBytes / 1024.0f / 1024.0f);
        kernel.drawText(etrText, x + 20, y + 85, 8, 0xFFAAAAAA);
        
        kernel.drawText(std::to_string((int)(s.progress * 100)) + "%", x + w - 50, y + 30, 10, 0xFFEAB308);
    }
};

} // namespace Aura::Graphics::UI
