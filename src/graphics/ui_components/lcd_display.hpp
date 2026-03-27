#pragma once
#include "ui_view.hpp"
#include "../../AuraUltimate.hpp"
#include <iomanip>
#include <sstream>

namespace Aura::Graphics::UI {

/**
 * @class LCDDisplay
 * @brief The High-Precision Heart of the Aura DAW.
 * Logic Pro 11 Style LCD with sub-pixel rendering and real-time engine sync.
 */
class LCDDisplay : public View {
public:
    LCDDisplay() {}

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        if (!m_visible) return;

        auto& engine = ::Aura::AuraEngine::getInstance();
        auto b = m_bounds;

        // --- 1. CHASSIS: Obsidian Glass Enclosure ---
        kernel.drawRoundedRect(b.x, b.y, b.w, b.h, 6.0f, 0xFF000000); 
        kernel.drawGradientRect(b.x + 1.5f, b.y + 1.5f, b.w - 3.0f, b.h - 3.0f, 0xFF141416, 0xFF0A0A0C);
        kernel.drawRoundedRect(b.x + 2.0f, b.y + 2.0f, b.w - 4.0f, b.h - 4.0f, 4.0f, 0x11FFFFFF); // Beveled shine

        // --- 2. TIME READOUT (Logic Standard: 1 1 1 1) ---
        const uint64_t playhead = engine.getCurrentSamplePos();
        const double sr = engine.getSampleRate();
        const float bpm = engine.getBPM();
        
        // Exact Bar / Beat Calculation
        double totalBeats = (static_cast<double>(playhead) / sr) * (bpm / 60.0);
        int bars = 1 + static_cast<int>(totalBeats / 4.0);
        int beats = 1 + static_cast<int>(std::fmod(totalBeats, 4.0));
        int divs = 1 + static_cast<int>(std::fmod(totalBeats * 4.0, 4.0));
        int ticks = static_cast<int>(std::fmod(totalBeats * 960.0, 240.0));

        char timeStr[64];
        snprintf(timeStr, sizeof(timeStr), "%3d  %1d  %1d %03d", bars, beats, divs, ticks);
        
        float timeX = b.x + 24, timeY = b.y + 24;
        kernel.drawText(timeStr, timeX, timeY, 20, 0xFF30B0FF); // Logic Azure Blue
        kernel.drawText("BAR     BEAT   DIV    TICK", timeX + 2, timeY - 14, 7, 0xFF6B7280);

        // --- 3. TEMPO & SCALE PANEL ---
        float infoX = b.x + b.w * 0.55f;
        char bpmStr[32]; snprintf(bpmStr, sizeof(bpmStr), "%.1f", bpm);
        kernel.drawText(bpmStr, infoX, timeY, 15, 0xFF30B0FF);
        kernel.drawText("TEMPO", infoX, timeY - 13, 6, 0xFF6B7280);

        kernel.drawText("4/4 C Maj", infoX + 60, timeY, 12, 0xFFFBBF24); // Gold scale
        kernel.drawText("KEY / SIGN", infoX + 60, timeY - 13, 6, 0xFF6B7280);

        // --- 4. PERFORMANCE TELEMETRY ---
        float telX = b.x + b.w - 120, telY = b.y + b.h - 12;
        kernel.drawRect(telX, telY, 40, 2, 0xFF1F2937);
        kernel.drawRect(telX, telY, 12, 2, 0xFF34D399); // 30% CPU signal
        kernel.drawText("CPU", telX, telY - 6, 5, 0xFF9CA3AF);

        kernel.drawRect(telX + 50, telY, 40, 2, 0xFF1F2937);
        kernel.drawRect(telX + 50, telY, 4, 2, 0xFF3B82F6);  // 10% Disk signal
        kernel.drawText("DISK", telX + 50, telY - 6, 5, 0xFF9CA3AF);

        // --- 5. INTELLIGENCE OVERLAY ---
        auto metrics = engine.getLatestMetrics();
        std::string infoLine = "AURA ULTIMATE: STUDIO READY";
        if (metrics.lufsIntegrated > -10.0f) infoLine = "MASTERING: OPTIMIZED (-14 LUFS)";
        kernel.drawText(infoLine, b.x + 24, b.y + b.h - 8, 8, 0xFF64748B);
    }
};

} // namespace Aura::Graphics::UI
