#pragma once
#include <string>
#include <cmath>
#include <algorithm>
#include "../graphics_kernel.hpp"
#include "aura_pro_icons.hpp"

namespace Aura::Graphics::UI {

/**
 * @struct TrackHeaderState
 * @brief High-fidelity metadata for studio rendering.
 */
struct TrackHeaderState {
    uint32_t id; std::string name; std::string type;
    bool muted; bool soloed; bool armed;
    float volume; float pan;
    float peakL; float peakR; uint32_t color; bool isSelected;
};

/**
 * @class TrackHeaderRenderer
 * @brief Professional High-Fidelity Track Component.
 * FIXED: REAL-TIME L/R Meter synchronization and Glassmorphism.
 */
class TrackHeaderRenderer {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const TrackHeaderState& state) {
        
        // --- 1. BASE: EBONY GLASS ---
        uint32_t bg = state.isSelected ? 0xFF2A2A2D : 0xFF1C1C1E;
        kernel.drawGlassRect(x, y, w, h, 0, bg);
        if (state.isSelected) kernel.drawRect(x, y, 4, h, 0xFF30B0FF); // Focus strip
        
        float margin = 10.0f;
        float iconS = 32.0f;
        
        // --- 2. ICON & NAME ---
        if (state.type == "audio") kernel.drawIconAudio(x + margin, y + 10, iconS, state.color);
        else kernel.drawIconInstrument(x + margin, y + 10, iconS, state.color);
        
        kernel.drawText(state.name, x + 50, y + 24, 12, 0xFFF1F5F9);
        kernel.drawText(state.type, x + 50, y + 36, 8, 0xFF94A3B8);

        // --- 3. CONTROLLERS: MUTE / SOLO ---
        float bx = x + 120, bw = 22, bh = 22;
        ProfessionalIcons::drawMute(kernel, bx, y + 10, bw, state.muted);
        ProfessionalIcons::drawSolo(kernel, bx + 28, y + 10, bw, state.soloed);
        ProfessionalIcons::drawRecord(kernel, bx + 56, y + 10, bw, state.armed);

        // --- 4. THE METER (Real-Time Synchronized) ---
        float mx = x + w - 36, mw = 12, mh = h - 20;
        kernel.drawMeter(state.id, state.peakL, state.peakR, mx, y + 10, mw, mh);
        
        // Fader Label (Numeric)
        char volStr[16]; snprintf(volStr, 16, "%.1f dB", 20.0f * std::log10(std::max(0.001f, state.volume)));
        kernel.drawText(volStr, x + 10, y + h - 14, 9, 0xFF64748B);

        // --- 5. BORDER ---
        kernel.drawLine(x, y + h - 1, x + w, y + h - 1, 1.0f, 0xFF000000);
    }
};

} // namespace Aura::Graphics::UI
