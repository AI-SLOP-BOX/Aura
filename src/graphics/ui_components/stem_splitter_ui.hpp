#pragma once
#include "ui_view.hpp"
#include "../../scae/AuraAISuite.hpp"

namespace Aura::Graphics::UI {

/**
 * @class StemSplitterUI
 * @brief Logic Pro 11 style Stem Splitter (Source Separation).
 * Separation into Vocals, Drums, Bass, and Other.
 */
class StemSplitterUI : public View {
public:
    StemSplitterUI() {
        m_visible = false;
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        if (!m_visible) return;
        auto b = m_bounds;

        // --- 1. GLASS OVERLAY ---
        kernel.applyBlurEffect(b.x, b.y, b.w, b.h, 20.0f);
        kernel.drawRoundedRect(b.x + b.w*0.5f - 250, b.y + b.h*0.5f - 180, 500, 360, 16.0f, 0xEE111115);
        kernel.drawRoundedRect(b.x + b.w*0.5f - 250, b.y + b.h*0.5f - 180, 500, 360, 16.0f, 0x33FFFFFF);

        float cx = b.x + b.w * 0.5f;
        float cy = b.y + b.h * 0.5f;

        // --- 2. HEADER: STEM SPLITTER ---
        kernel.drawText("STEM SPLITTER", cx - 80, cy - 140, 18, 0xFFFFFFFF);
        kernel.drawText("Choose the stems you want to extract from the region.", cx - 140, cy - 115, 9, 0xFF9CA3AF);

        // --- 3. THE 4 STEMS (Icons) ---
        float iconSize = 80.0f;
        float spacing = 20.0f;
        float startX = cx - (iconSize * 2 + spacing * 1.5f);
        
        const char* labels[] = {"VOCALS", "DRUMS", "BASS", "OTHER"};
        uint32_t colors[] = {0xFF30B0FF, 0xFFEAB308, 0xFF34C759, 0xFFF87171};

        for (int i = 0; i < 4; ++i) {
            float ix = startX + i * (iconSize + spacing);
            float iy = cy - 40;
            
            bool selected = (m_selectedMask & (1 << i));
            kernel.drawRoundedRect(ix, iy, iconSize, iconSize, 8.0f, selected ? colors[i] : 0xFF1F2937);
            kernel.drawText(labels[i], ix + (iconSize - strlen(labels[i])*6)/2, iy + iconSize + 15, 8, selected ? 0xFFFFFFFF : 0xFF6B7280);
            
            // Checkmark if selected
            if (selected) {
                 kernel.drawCircle(ix + iconSize - 10, iy + 10, 6, 0xFFFFFFFF);
                 kernel.drawCircle(ix + iconSize - 10, iy + 10, 4, colors[i]);
            }
        }

        // --- 4. ACTION BUTTON ---
        float btnW = 180, btnH = 36;
        float btnX = cx - btnW*0.5f, btnY = cy + 100;
        
        if (m_isProcessing) {
            kernel.drawRoundedRect(btnX, btnY, btnW, btnH, 6.0f, 0xFF1F2937);
            kernel.drawText("SPLITTING...", btnX + 55, btnY + 22, 10, 0xFFFFFFFF);
            kernel.drawRect(btnX, btnY + btnH - 2, btnW * m_progress, 2, 0xFF30B0FF);
        } else {
            kernel.drawRoundedRect(btnX, btnY, btnW, btnH, 6.0f, 0xFF3B82F6);
            kernel.drawText("SPLIT STEMS", btnX + 55, btnY + 22, 10, 0xFFFFFFFF);
        }
    }

    bool onMouseDown(float x, float y) override {
        auto b = m_bounds;
        float cx = b.x + b.w * 0.5f;
        float cy = b.y + b.h * 0.5f;
        
        // Handle Icon Selection
        float iconSize = 80.0f, spacing = 20.0f;
        float startX = cx - (iconSize * 2 + spacing * 1.5f);
        for (int i = 0; i < 4; ++i) {
            float ix = startX + i * (iconSize + spacing);
            float iy = cy - 40;
            if (x >= ix && x <= ix + iconSize && y >= iy && y <= iy + iconSize) {
                m_selectedMask ^= (1 << i);
                return true;
            }
        }

        // Handle Split Button
        float btnW = 180, btnH = 36;
        float btnX = cx - btnW*0.5f, btnY = cy + 100;
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            startSeparation();
            return true;
        }

        // Click outside panel to close
        if (x < b.x + b.w*0.5f - 250 || x > b.x + b.w*0.5f + 250 ||
            y < b.y + b.h*0.5f - 180 || y > b.y + b.h*0.5f + 180) {
            m_visible = false;
        }
        
        return true; 
    }

    void startSeparation() {
        m_isProcessing = true;
        m_progress = 0.0f;
        // Mock progress
        std::thread([this]() {
            for (int i = 0; i <= 100; ++i) {
                m_progress = i / 100.0f;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            m_isProcessing = false;
            m_visible = false;
            // logic to create new tracks here...
        }).detach();
    }

private:
    uint8_t m_selectedMask = 0x0F; // All selected by default
    bool m_isProcessing = false;
    float m_progress = 0.0f;
};

} // namespace Aura::Graphics::UI
