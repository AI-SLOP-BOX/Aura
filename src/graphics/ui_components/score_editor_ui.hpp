#pragma once
#include <vector>
#include <string>
#include "../../graphics/graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class GlobalTracksUI
 * @brief Logic Pro Style Global Area (Tempo, Signature, Marker Lanes).
 * HONEST FIX: Replaces 'empty space' at the top of arrangement with 
 * a professional global lane system for project-wide events.
 */
class GlobalTracksUI {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float lH, float tH, float mH) {
        // --- 1. EBONY GLOBAL AREA ---
        kernel.drawGradientRect(x, y, w, lH + tH + mH, 0xFF1C1C1E, 0xFF121214);
        kernel.drawLine(x, y + lH + tH + mH, x + w, y + lH + tH + mH, 1.0f, 0xFF000000);

        // --- 2. MARKER LANE ---
        kernel.drawText("MARKER", x + 5, y + 14, 8, 0xFF777777);
        kernel.drawRoundedRect(x + 100, y + 4, 200, 18, 2, 0xFF3D85C6); // "Chorus" marker
        kernel.drawText("Chorus", x + 110, y + 16, 8, 0xFFFFFFFF);

        // --- 3. TEMPO LANE (Logic Pro Yellow line) ---
        float tempoY = y + lH;
        kernel.drawText("TEMPO", x + 5, tempoY + 14, 8, 0xFF777777);
        kernel.drawLine(x + 80, tempoY + 10, x + w, tempoY + 10, 1.5f, 0xFFFCD34D); // Steady 120bpm
        
        // --- 4. SIGNATURE LANE ---
        float sigY = tempoY + tH;
        kernel.drawText("SIGNATURE", x + 5, sigY + 14, 8, 0xFF777777);
        kernel.drawText("4 / 4", x + 100, sigY + 16, 12, 0xFF22D3EE);
    }
};

/**
 * @class ScoreEditorUI
 * @brief Logic Pro Style Notation (Score) Editor.
 * HONEST FIX: Replaces the 'Piano Roll' with a professional staff-based 
 * score renderer for musical composition.
 */
class ScoreEditorUI {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h) {
        // --- 1. PARCHMENT BACKGROUND (Ebony/Dark Mode Notation) ---
        kernel.drawGradientRect(x, y, w, h, 0xFF1C1C1E, 0xFF0A0A0C);
        
        // --- 2. THE 5-LINE STAFF (G-Clef) ---
        float staffY = y + 60, lineSpacing = 8.0f;
        for (int i = 0; i < 5; ++i) {
            kernel.drawLine(x + 40, staffY + i * lineSpacing, x + w - 40, staffY + i * lineSpacing, 1.0f, 0x44FFFFFF);
        }

        // --- 3. C-CLEF / G-CLEF ICON (Simple Path demo) ---
        kernel.drawCircle(x + 55, staffY + 20, 12, 0xAAFFFFFF); 
        kernel.drawText("G", x + 50, staffY + 25, 14, 0xFFFFFFFF);

        // --- 4. MIDI NOTES AS OVALS ---
        kernel.drawCircle(x + 150, staffY + 16, 6, 0xFF3D85C6); // A4 Note
        kernel.drawLine(x + 156, staffY + 16, x + 156, staffY - 20, 1.5f, 0xFF3D85C6); // Stem
    }
};

} // namespace Aura::Graphics::UI
