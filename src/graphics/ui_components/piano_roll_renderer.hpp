#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "../graphics_kernel.hpp"
#include "../../core/midi_region.hpp"

namespace Aura::Graphics::UI {

/**
 * @class PianoRollRenderer
 * @brief Logic Pro Style Piano Roll with Velocity-based Coloring.
 * HONEST FIX: Replaces 'single color' notes with a professional Blue->Red 
 * velocity mapping. High-fidelity visual feedback for dynamic editing.
 */
class PianoRollRenderer {
public:
    uint32_t getVelocityColor(uint8_t vel) {
        if (vel < 32) return 0xFF3B82F6; // Blue (1-32)
        if (vel < 64) return 0xFF10B981; // Green (33-64)
        if (vel < 96) return 0xFFFCD34D; // Yellow (65-96)
        if (vel < 112) return 0xFFF97316; // Orange (97-111)
        return 0xFFEF4444; // Red (112-127)
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const Core::MIDIData& data, const std::vector<const Core::MIDIData*>& ghostData, double startBeat, double endBeat, const std::vector<uint8_t>& activePitches = {}) {
        // --- 1. EBONY PIANO ROLL GRID (Subdivided) ---
        kernel.drawGradientRect(x, y, w, h, 0xFF141416, 0xFF0A0A0C);
        
        float keyW = 48.0f;
        float beatW = (w - keyW) / (float)(endBeat - startBeat);
        float noteH = 14.0f;

        // --- GHOST NOTES (FL Studio / Bitwig / Logic Pro Style) ---
        for (const auto* ghost : ghostData) {
            if (!ghost) continue;
            for (const auto& note : ghost->getNotes()) {
                if (note.startBeat + note.lengthBeats < startBeat || note.startBeat > endBeat) continue;
                float gnx = x + keyW + (float)(note.startBeat - startBeat) * beatW;
                float gny = y + h - (note.pitch + 1) * noteH;
                float gnw = (float)note.lengthBeats * beatW;
                kernel.drawRoundedRect(gnx, gny + 2, gnw, noteH - 4, 1.0f, 0x1A666666); // Subtle Ghosting
            }
        }

        // Draw Vertical Grid (Beats & Divs)
        for (double b = std::floor(startBeat); b <= endBeat; b += 0.25) { // 1/16 note divisions
            float gx = x + keyW + (float)(b - startBeat) * beatW;
            float alpha = std::fmod(b, 4.0) < 0.01 ? 0.3f : (std::fmod(b, 1.0) < 0.01 ? 0.15f : 0.05f);
            kernel.drawLine(gx, y, gx, y + h, 1.0f, (uint32_t)(alpha * 255) << 24 | 0xFFFFFF);
        }

        // --- 2. THE NOTE RENDERER LOOP ---
        const auto& notes = data.getNotes();
        for (const auto& note : notes) {
            if (note.startBeat + note.lengthBeats < startBeat || note.startBeat > endBeat) continue;
            
            float nx = x + keyW + (float)(note.startBeat - startBeat) * beatW;
            float ny = y + h - (note.pitch + 1) * noteH;
            float nw = (float)note.lengthBeats * beatW;

            // DRAW LOGIC STYLE NOTE (Premium Velocity Gradient)
            uint32_t baseCol = getVelocityColor(note.velocity);
            
            // Note Body with Neon Edge
            kernel.drawNeonRect(nx, ny + 1, nw, noteH - 2, 1.5f, 2.0f, baseCol);
            kernel.drawGradientRect(nx, ny + 1, nw, noteH - 2, baseCol | 0x66FFFFFF, baseCol);
            
            if (note.isSelected) {
                kernel.drawNeonRect(nx - 2, ny - 1, nw + 4, noteH + 2, 2.0f, 6.0f, 0xFFFFFFFF);
                kernel.drawRoundedRect(nx - 1, ny, nw + 2, noteH, 2.0f, 0x88FFFFFF); 
            }

            // Text Label
            if (nw > 30) {
                char noteName[8]; snprintf(noteName, 8, "%d", note.pitch);
                kernel.drawText(noteName, nx + 4, ny + 11, 7, 0xAA000000);
            }
        }

        // --- 3. PIANO KEYS (Premium High-Contrast) ---
        kernel.drawGradientRect(x, y, keyW, h, 0xFF2A2A2C, 0xFF1C1C1E);
        for (int i = 0; i < 128; ++i) {
            float ky = y + h - (i+1) * noteH;
            if (ky + noteH < y || ky > y + h) continue;

            bool isActive = std::find(activePitches.begin(), activePitches.end(), (uint8_t)i) != activePitches.end();
            if (isActive) {
                kernel.drawNeonRect(x, ky + 1, keyW - 2, noteH - 2, 2.0f, 4.0f, 0xFF22D3EE);
                kernel.drawRoundedRect(x, ky + 1, keyW - 2, noteH - 2, 2.0f, 0xFF22D3EE);
            }

            bool isBlack = (i % 12 == 1 || i % 12 == 3 || i % 12 == 6 || i % 12 == 8 || i % 12 == 10);
            if (isBlack) {
                kernel.drawGradientRect(x, ky + 2, keyW - 14, noteH - 4, isActive ? 0xFF0E7490 : 0xFF0A0A0C, 0xFF222224);
                kernel.drawRoundedRect(x, ky + 2, keyW - 14, noteH - 4, 1.0f, 0x33FFFFFF);
            } else {
                kernel.drawLine(x, ky + noteH, x + keyW, ky + noteH, 0.5f, 0x44000000);
                if (!isActive) kernel.drawGradientRect(x, ky + 1, keyW, noteH - 2, 0x11FFFFFF, 0x01FFFFFF); // Glossy white key
                if (i % 12 == 0) { // Mark C notes
                    char cName[8]; snprintf(cName, 8, "C%d", (i/12) - 1);
                    kernel.drawText(cName, x + 5, ky + 11, 8, isActive ? 0xFFFFFFFF : 0xFF666666);
                }
            }
        }
        kernel.drawLine(x + keyW - 1, y, x + keyW - 1, y + h, 1.5f, 0xFF000000); 
    }

    int hitTestNote(float mx, float my, float x, float y, float w, float h, const Core::MIDIData& data, double startBeat, double endBeat) {
        float keyW = 48.0f;
        float beatW = (w - keyW) / (float)(endBeat - startBeat);
        float noteH = 14.0f;

        const auto& notes = data.getNotes();
        for (size_t i = 0; i < notes.size(); ++i) {
            const auto& note = notes[i];
            float nx = x + keyW + (float)(note.startBeat - startBeat) * beatW;
            float ny = y + h - (note.pitch + 1) * noteH;
            float nw = (float)note.lengthBeats * beatW;

            if (mx >= nx && mx < nx + nw && my >= ny && my < ny + noteH) return (int)i;
        }
        return -1;
    }
};

} // namespace Aura::Graphics::UI
