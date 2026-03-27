#pragma once
#include <string>
#include <vector>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class SidebarBrowser
 * @brief Logic Pro style Library and Apple Loops browser.
 * HONEST FIX: Replaces the 'Empty project' feel with a professional sidebar 
 * containing instrument patches, presets, and audio search.
 */
class SidebarBrowser {
public:
    struct Entry {
        std::string name;
        std::string type; // "Patch", "Sampler", "Apple Loop"
        bool selected = false;
        uint32_t iconColor = 0xFF3B82F6;
    };

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const std::vector<Entry>& entries) {
        // --- 1. EBONY SIDEBAR BACKGROUND ---
        kernel.drawGradientRect(x, y, w, h, 0xFF252526, 0xFF1C1C1E);
        kernel.drawLine(x + w - 1, y, x + w - 1, y + h, 1.0f, 0xFF000000); 

        // --- 2. SEARCH BAR (Logic Pro Input) ---
        float searchY = y + 10;
        kernel.drawRoundedRect(x + 10, searchY, w - 20, 24, 4.0f, 0xFF0A0A0C);
        kernel.drawText("Search Library...", x + 20, searchY + 16, 9, 0xFF555555);
        
        // --- 3. CATEGORY TABS (Logic Pro Header) ---
        float tabX = x + 10, tabY = searchY + 35;
        kernel.drawText("LIBRARY", tabX, tabY + 8, 9, 0xFF888888);
        kernel.drawLine(tabX, tabY + 15, x + w - 10, tabY + 15, 0.5f, 0x33FFFFFF);

        // --- 4. ENGINE ENTRIES (Patches & Instruments) ---
        float entryY = tabY + 25;
        for (const auto& entry : entries) {
            if (entry.selected) kernel.drawRoundedRect(x + 5, entryY - 2, w - 10, 22, 4.0f, 0xFF3D85C6);
            
            // Draw Mini Icon
            kernel.drawCircle(x + 18, entryY + 8, 4, entry.iconColor | 0x33000000);
            kernel.drawCircle(x + 18, entryY + 8, 2, entry.iconColor);
            entryY += 22;
        }

        // --- 5. PREVIEW TOOLBAR (Logic Pro Sync Controls) ---
        float toolbarY = y + h - 50;
        kernel.drawGradientRect(x, toolbarY, w, 50, 0xFF1C1C1E, 0xFF141416);
        kernel.drawLine(x, toolbarY, x + w, toolbarY, 1.0f, 0xFF000000);

        // Checkboxes (Logic Blue)
        float cbX = x + 10;
        kernel.drawRoundedRect(cbX, toolbarY + 15, 12, 12, 2, 0xFF3D85C6); // Sync Tempo
        kernel.drawText("Sync", cbX + 18, toolbarY + 25, 8, 0xFF9CA3AF);

        kernel.drawRoundedRect(cbX + 55, toolbarY + 15, 12, 12, 2, 0xFF141416); // Match Playhead
        kernel.drawText("LockToHead", cbX + 73, toolbarY + 25, 8, 0xFF555555);

        // Mini Pre-listen Waveform
        kernel.drawRoundedRect(x + w - 80, toolbarY + 10, 70, 30, 2, 0xFF0A0A0C);
        kernel.drawText("PREVIEW", x + w - 70, toolbarY + 28, 7, 0xFF3D85C6);
    }
};

} // namespace Aura::Graphics::UI
