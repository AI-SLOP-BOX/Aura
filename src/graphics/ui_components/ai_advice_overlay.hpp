#pragma once
#include <vector>
#include <string>
#include "../graphics_kernel.hpp"
#include "../../core/aura_unified_engine.hpp"

namespace Aura::Graphics::UI {

/**
 * @class MixingMetricsOverlay
 * @brief Calculation-based Mixing metrics and advice.
 * Purely mathematical advice based on LUFS and Peak analysis.
 */
class AIAdviceOverlay {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h) {
        auto& engine = Core::Engine::AuraUnifiedEngine::getInstance();
        float currentLUFS = engine.getCurrentLUFS();
        float currentPeak = engine.getCurrentTruePeak();

        float panelW = 280.0f;
        float panelX = x + w - panelW - 30.0f;
        float panelY = y + 40.0f;

        // 1. NEON GLASS PANEL (Backdrop blur)
        kernel.applyBlurEffect(panelX, panelY, panelW, 140, 15.0f);
        kernel.drawRoundedRect(panelX, panelY, panelW, 140, 12.0f, 0xAA0F0F15); // Deep Charcoal
        
        // 2. HEADER: "MIXING METRICS & ADVICE" (Professional Blue)
        kernel.drawText("MIXING METRICS & ADVICE", panelX + 20, panelY + 25, 11, 0xFF58C1FF);
        kernel.drawLine(panelX + 20, panelY + 35, panelX + panelW - 20, panelY + 35, 1.0f, 0x3358C1FF);

        // 3. REAL-TIME PERFORMANCE METRICS
        char lufsStr[32], peakStr[32], drStr[32];
        snprintf(lufsStr, sizeof(lufsStr), "LOUDNESS: %.1f LUFS", currentLUFS);
        snprintf(peakStr, sizeof(peakStr), "TRUE PEAK: %.2f dB", currentPeak);
        
        float dynamicRange = currentPeak - currentLUFS;
        snprintf(drStr, sizeof(drStr), "CREST FACTOR: %.1f dB", dynamicRange);

        kernel.drawText(lufsStr, panelX + 20, panelY + 50, 10, 0xFFEEEEEE);
        kernel.drawText(peakStr, panelX + 20, panelY + 65, 10, 0xFFEEEEEE);
        kernel.drawText(drStr, panelX + 20, panelY + 80, 9, 0xFF888888);

        // 4. ACTIONABLE ADVICE (DSP Calculation based)
        if (currentPeak > -0.1f) {
            drawAdvice(kernel, panelX + 20, panelY + 105, "CRITICAL: CLIPPING", "Lower Master Gain to prevent dist.", 0xFFFF3B30);
        } else if (dynamicRange < 8.0f && currentLUFS > -12.0f) {
            drawAdvice(kernel, panelX + 20, panelY + 105, "LOW DYNAMICS", "Crest factor is narrow. Reduce compression.", 0xFFFF9500);
        } else if (currentLUFS < -16.0f) {
            drawAdvice(kernel, panelX + 20, panelY + 105, "HEADROOM READY", "Plenty of room. Consider a 2dB boost.", 0xFF58C1FF);
        } else {
            drawAdvice(kernel, panelX + 20, panelY + 105, "OPTIMAL BALANCE", "Levels meet professional standards.", 0xFF34C759);
        }
    }

private:
    void drawAdvice(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, const char* title, const char* desc, uint32_t accent) {
        kernel.drawRect(x, y, 3, 30, (accent & 0x00FFFFFF) | 0x44000000); 
        kernel.drawRect(x + 1, y, 1, 30, accent); 
        
        kernel.drawText(title, x + 12, y + 10, 10, accent);
        kernel.drawText(desc, x + 12, y + 25, 9, 0xFFAAAAAA);
    }
};

} // namespace Aura::Graphics::UI

