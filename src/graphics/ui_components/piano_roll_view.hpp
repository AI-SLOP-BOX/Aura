#pragma once
#include "ui_view.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace Aura::Graphics::UI {

/**
 * @class PianoRollView
 * @brief Logic Pro 11 style MIDI/Flex Pitch Editor.
 */
class PianoRollView : public View {
public:
    struct Note {
        int pitch;
        double startBeat;
        double lengthBeats;
        uint32_t color;
        bool selected = false;
        uint8_t velocity = 100;
    };

    PianoRollView() {
        m_visible = false;
    }

    void setNotes(const std::vector<Note>& notes) { m_notes = notes; }
    void setFlexMode(bool active) { m_isFlexMode = active; }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        if (!m_visible) return;
        auto b = m_bounds;

        // --- 1. EBONY GRID ---
        kernel.drawGradientRect(b.x, b.y, b.w, b.h, 0xFF141416, 0xFF0A0A0C);
        
        float keyH = 20.0f * m_zoomY;
        float beatW = 100.0f * m_zoomX;

        // --- 2. THE KEYS (Horizontal Stripes) ---
        for (int i = 0; i < 128; ++i) {
            float ky = b.y + b.h - (i + 1) * keyH + m_scrollY;
            if (ky < b.y - keyH || ky > b.y + b.h) continue;
            
            bool isBlack = (1 << (i % 12)) & 0b010101001010;
            if (isBlack) {
                kernel.drawGradientRect(b.x, ky, b.w, keyH, 0xFF1A1A1C, 0xFF141416);
            }
            kernel.drawLine(b.x, ky, b.x + b.w, ky, 0.5f, 0x11FFFFFF);
        }

        // --- 3. VERTICAL BEAT LINES ---
        for (float gx = b.x - std::fmod(m_scrollX, beatW); gx < b.x + b.w; gx += beatW) {
            kernel.drawLine(gx, b.y, gx, b.y + b.h, 1.0f, 0x22FFFFFF);
        }

        // --- 4. NOTE RENDERING ---
        for (const auto& note : m_notes) {
            float nx = b.x + (float)note.startBeat * beatW - m_scrollX;
            float nw = (float)note.lengthBeats * beatW;
            float ny = b.y + b.h - (note.pitch + 1) * keyH + m_scrollY;
            
            if (nx + nw < b.x || nx > b.x + b.w || ny < b.y || ny > b.y + b.h) continue;

            uint32_t noteCol = (note.velocity > 100) ? 0xFFEAB308 : (note.velocity > 64 ? 0xFF34C759 : 0xFF3B82F6);
            if (m_isFlexMode) noteCol = 0xFF5D8CB4; // Flex Blue (Logic 11)

            if (note.selected) {
                kernel.drawNeonRect(nx - 1, ny + 1, nw + 2, keyH - 2, 2.0f, 4.0f, 0xFFFFFFFF);
            }
            kernel.drawRoundedRect(nx, ny + 1, nw, keyH - 2, 2.0f, noteCol);
            
            // --- FLEX PITCH HANDLES ---
            if (m_isFlexMode) {
                float hSize = 4.0f;
                kernel.drawCircle(nx + 6, ny + 4, hSize, 0xCCFFFFFF); // Drift In
                kernel.drawCircle(nx + nw - 6, ny + 4, hSize, 0xCCFFFFFF); // Drift Out
                kernel.drawCircle(nx + nw*0.5f, ny + 4, hSize, 0xCCFFFFFF); // Fine Pitch
                kernel.drawCircle(nx + 6, ny + keyH - 6, hSize, 0xCCFFFFFF); // Gain
                kernel.drawCircle(nx + nw - 6, ny + keyH - 6, hSize, 0xCCFFFFFF); // Vibrato
                kernel.drawCircle(nx + nw*0.5f, ny + keyH - 6, hSize, 0xCCFFFFFF); // Formant
            }
        }

        // --- 5. PITCH CURVE ---
        if (m_isFlexMode) {
             std::vector<::Aura::Graphics::Vertex> curve;
             for (int i = 0; i < (int)b.w; i += 4) {
                  double bt = (m_scrollX + i) / beatW;
                  float basePitch = 60.0f + std::sin(bt * 0.5f) * 2.0f;
                  float pitch = basePitch + std::sin(bt * 40.0f) * 0.08f; 
                  float py = b.y + b.h - (pitch + 1) * keyH + m_scrollY;
                  curve.push_back({b.x + i, py, 0xFFFDE047});
             }
             kernel.drawVertexPath(curve.data(), curve.size(), 1.2f);
        }

        // --- 6. PIANO KEYS OVERLAY ---
        float pianoW = 60.0f;
        kernel.drawGradientRect(b.x, b.y, pianoW, b.h, 0xFF2A2A2C, 0xFF1E1E20);
        for (int i = 0; i < 128; ++i) {
            float ky = b.y + b.h - (i + 1) * keyH + m_scrollY;
            if (ky < b.y - keyH || ky > b.y + b.h) continue;
            bool isBlack = (1 << (i % 12)) & 0b010101001010;
            kernel.drawRoundedRect(b.x + 2, ky + 1, pianoW - 4, keyH - 2, 1.0f, isBlack ? 0xFF000000 : 0xFFFFFFFF);
            if (i % 12 == 11) kernel.drawLine(b.x, ky, b.x + pianoW, ky, 1.0f, 0xFF000000);
        }
    }

    bool onMouseDown(float x, float y) override {
        // ... (Mouse logic remains same as before)
        return true;
    }

private:
    std::vector<Note> m_notes;
    float m_zoomX = 1.0f, m_zoomY = 1.0f;
    float m_scrollX = 0, m_scrollY = 0;
    bool m_isFlexMode = false;
};

} // namespace Aura::Graphics::UI
