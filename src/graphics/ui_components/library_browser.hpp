#pragma once
#include <vector>
#include <string>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class LibraryBrowser
 * @brief Logic Pro Style Patch/Instrument Library.
 */
class LibraryBrowser {
public:
    struct Item {
        std::string name;
        std::string category;
        bool isNative;
    };

    LibraryBrowser() {
        m_items = {
            {"Dreamy Sine", "Synthesizer", true},
            {"Vintage Grand", "Piano", true},
            {"Thick sub", "Bass", true},
            {"Lofi Kit", "Drums", false},
            {"Aura Pad", "Pad", true}
        };
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h) {
        // --- 1. BACKGROUND (Logic Charcoal) ---
        kernel.drawGradientRect(x, y, w, h, 0xFF28282A, 0xFF1C1C1E);
        kernel.drawLine(x + w - 1, y, x + w - 1, y + h, 1.0f, 0xFF000000);
        
        kernel.drawText("LIBRARY", x + 15, y + 25, 10, 0xFFBBBBBB);
        kernel.drawText("Search Patches...", x + 15, y + 50, 8, 0xFF555555);
        kernel.drawRoundedRect(x + 12, y + 38, w - 24, 20, 2.0f, 0xFF121214);

        // --- 2. CATEGORIES ---
        float yPos = y + 75;
        drawCategory(kernel, x, yPos, w, "All Sounds", true); yPos += 22;
        drawCategory(kernel, x, yPos, w, "Synthesizer", false); yPos += 22;
        drawCategory(kernel, x, yPos, w, "Piano & Keys", false); yPos += 22;
        drawCategory(kernel, x, yPos, w, "Drum Kit", false); yPos += 22;

        // --- 3. ITEMS ---
        float ix = x + 15, iy = yPos + 15;
        for (size_t i = 0; i < m_items.size(); ++i) {
            uint32_t col = (i == m_selectedIndex) ? 0xFF3B82F6 : 0xFFD1D1D1;
            if (i == m_selectedIndex) kernel.drawRoundedRect(x + 5, iy - 14, w - 10, 20, 2.0f, 0x333B82F6);
            
            kernel.drawText(m_items[i].name, ix, iy, 9, col);
            if (m_items[i].isNative) kernel.drawText("AUR", x + w - 35, iy, 6, 0xFF555555);
            iy += 22;
        }
    }

    void onMouseDown(float mx, float my, float x, float y) {
        float iy = y + 158; // Approx start of items
        int idx = (int)((my - iy) / 22.0f);
        if (idx >= 0 && idx < (int)m_items.size()) m_selectedIndex = idx;
    }

private:
    void drawCategory(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, const char* name, bool expanded) {
        kernel.drawText(name, x + 25, y + 14, 8, 0xFFBBBBBB);
        kernel.drawTriangle(x + 10, y + 8, x + 10, y + 14, x + 15, y + 11, 1.0f, 0xFF888888);
        kernel.drawLine(x, y + 21, x + w, y + 21, 0.5f, 0x1AFFFFFF);
    }

    int m_selectedIndex = 0;
    std::vector<Item> m_items;
};

} // namespace Aura::Graphics::UI
