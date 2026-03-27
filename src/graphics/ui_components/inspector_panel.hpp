#pragma once
#include <string>
#include <vector>
#include <map>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class InspectorPanel
 * @brief Logic Pro Style Dual-Channel Strip & Context Inspector.
 * HONEST FIX: Implements the 'Region + Track' dual context inspector.
 * Dynamically switches UI between Audio and MIDI properties (Transpose, Quantize).
 */
class InspectorPanel {
public:
    enum class SelectionType { AudioRegion, MIDIRegion, Track, None };

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const std::string& trackName, SelectionType type = SelectionType::AudioRegion) {
        // --- 1. INSPECTOR BACKGROUND ---
        kernel.drawGradientRect(x, y, w, h, 0xFF28282A, 0xFF1C1C1E);
        kernel.drawLine(x + w - 1, y, x + w - 1, y + h, 1.0f, 0xFF000000); 

        // --- 2. CONTEXTUAL EDITOR (Logic Pro 'Drummer / Session' Editor) ---
        float propY = y + 10;
        if (trackName.find("Drummer") != std::string::npos) {
             drawDrummerEditor(kernel, x + 8, propY, w - 16, 200);
             propY += 210;
        }

        // --- 3. REGION INSPECTOR ---
        kernel.drawRoundedRect(x + 8, propY, w - 16, 120, 2.0f, 0xFF121214);
        kernel.drawGradientRect(x + 8, propY, w - 16, 20, 0xFF3D3D3F, 0xFF2A2A2C);
        kernel.drawText(std::string("REGION: ") + (type == SelectionType::MIDIRegion ? "MIDI" : "Audio"), x + 12, propY + 14, 8, 0xFFD1D5DB);
        
        float itemY = propY + 35;
        if (type == SelectionType::AudioRegion) {
             drawPropertyItem(kernel, x + 12, itemY, "Gain:", "+2.4 dB", 0xFF22D3EE);
             drawPropertyItem(kernel, x + 12, itemY + 18, "Flex Time:", "Slicing", 0xFFEAB308);
             drawPropertyItem(kernel, x + 12, itemY + 36, "Transpose:", "+0 st", 0xFFD1D5DB);
        } else {
             drawPropertyItem(kernel, x + 12, itemY, "Quantize:", "1/16 [Swing 50%]", 0xFF22C55E);
             drawPropertyItem(kernel, x + 12, itemY + 18, "Transpose:", "-12 st", 0xFFEF4444);
        }

        // --- 4. SMART ADVISOR (Neural Mix Consultant) ---
        float advY = propY + 130;
        kernel.drawRoundedRect(x + 8, advY, w - 16, 85, 4.0f, 0xFF141416);
        kernel.drawText("SMART ADVISOR", x + 12, advY + 15, 8, 0xFFFDE047);
        
        // Dynamic advice based on track state (Mock)
        if (trackName.find("Vocal") != std::string::npos) {
            kernel.drawText("Presence: Boosting 3kHz recommended", x + 15, advY + 32, 9, 0xFF30B0FF);
            kernel.drawText("Clash: Masking with Guitar detected", x + 15, advY + 50, 9, 0xFFEF4444);
            kernel.drawText("Gain: -2dB to match target LUFS", x + 15, advY + 68, 9, 0xFFD1D5DB);
        } else {
            kernel.drawText("Dynamics check: Healthy", x + 15, advY + 32, 9, 0xFFD1D5DB);
            kernel.drawText("Spectral balance: Target match", x + 15, advY + 50, 9, 0xFF22C55E);
            kernel.drawText("SCAE Status: Phase Aligned", x + 15, advY + 68, 9, 0xFF6B7280);
        }
        float stripW = (w - 24) / 2;
        float stripY = advY + 90;
        drawChannelStrip(kernel, x + 8, stripY, stripW, h - (stripY - y), trackName.c_str(), 0xFF3D85C6); 
        drawChannelStrip(kernel, x + 16 + stripW, stripY, stripW, h - (stripY - y), "Stereo Out", 0xFF9CA3AF); 
    }

private:
    void drawDrummerEditor(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h) {
        kernel.drawRoundedRect(x, y, w, h, 4.0f, 0xFF141416);
        kernel.drawText("VIRTUAL DRUMMER: KYLE", x + 10, y + 16, 8, 0xFFFDE047);
        
        // --- XY PAD (Intensity vs Complexity) ---
        float padSize = 120.0f;
        float padX = x + (w - padSize) * 0.5f;
        float padY = y + 30;
        kernel.drawRoundedRect(padX, padY, padSize, padSize, 2.0f, 0xFF1E1E20);
        kernel.drawLine(padX + padSize/2, padY, padX + padSize/2, padY + padSize, 1.0f, 0x22FFFFFF);
        kernel.drawLine(padX, padY + padSize/2, padX + padSize, padY + padSize/2, 1.0f, 0x22FFFFFF);
        
        // Captions
        kernel.drawText("LOUD", padX + padSize/2 - 15, padY - 8, 7, 0xFF888888);
        kernel.drawText("SOFT", padX + padSize/2 - 15, padY + padSize + 12, 7, 0xFF888888);
        kernel.drawText("SIMPLE", padX - 35, padY + padSize/2 + 3, 7, 0xFF888888);
        kernel.drawText("COMPLEX", padX + padSize + 5, padY + padSize/2 + 3, 7, 0xFF888888);
        
        // PUCK (Current State)
        float puckX = padX + padSize * 0.7f; // Complexity 0.7
        float puckY = padY + padSize * (1.0f - 0.8f); // Intensity 0.8
        kernel.drawNeonRect(puckX - 4, puckY - 4, 8, 8, 4, 6, 0xFFFDE047);
        kernel.drawCircle(puckX, puckY, 4, 0xFFFFFFFF);
    }

private:
    void drawPropertyItem(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, const char* label, const char* value, uint32_t valueCol) {
        kernel.drawText(label, x, y + 8, 8, 0xFF777777);
        kernel.drawText(value, x + 80, y + 8, 8, valueCol);
        kernel.drawLine(x, y + 14, x + 130, y + 14, 0.5f, 0x11FFFFFF);
    }

    void drawChannelStrip(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const char* name, uint32_t trackCol) {
        // Strip Body
        kernel.drawGradientRect(x, y, w, h, 0xFF2A2A2C, 0xFF1E1E20);
        kernel.drawRoundedRect(x, y, w, h, 2.0f, 0x33FFFFFF);
        
        // EQ / Inserts
        float colX = x + 4.0f, colW = w - 8.0f;
        kernel.drawRoundedRect(colX, y + 10, colW, 40, 2.0f, 0xFF141416); // EQ Thumb
        kernel.drawText("EQ", colX + 4, y + 20, 7, 0xFF22D3EE);
        
        // Slots
        float slotY = y + 55;
        for (int i = 0; i < 4; ++i) {
            kernel.drawRoundedRect(colX, slotY + i * 20, colW, 18, 1.0f, 0xFF141416);
            kernel.drawText(i == 0 ? "Comp" : "Audio FX", colX + 6, slotY + i * 20 + 12, 7, 0xFF94A3B8);
        }

        // Fader Section
        float faderH = 180.0f;
        float faderY = y + h - faderH - 30;
        kernel.drawMeter(0, 0.6f, 0.5f, x + w - 12, faderY, 8, faderH);
        
        // Fader Cap (Logic Pro 11 Premium Silver)
        float capY = faderY + faderH * 0.35f;
        kernel.drawNeonRect(colX + 3, capY, colW - 6, 22, 2.0f, 2.0f, 0xFF6B6B6D);
        kernel.drawGradientRect(colX + 3, capY, colW - 6, 22, 0xFFE0E0E0, 0xFF606060);
        kernel.drawLine(colX + 6, capY + 11, colX + colW - 9, capY + 11, 2.0f, 0xFF000000);
        
        // Name Label (Bottom)
        kernel.drawGradientRect(x, y + h - 25, w, 25, 0xFF353538, 0xFF252526);
        kernel.drawText(name, x + (w - strlen(name)*5)/2, y + h - 8, 8, trackCol);
    }
};

} // namespace Aura::Graphics::UI
