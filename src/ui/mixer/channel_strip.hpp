#pragma once
#include <string>
#include <vector>
#include "../../graphics/graphics_kernel.hpp"
#include "../../graphics/ui_components/aura_pro_icons.hpp"
#include "../../core/engine/track.hpp"

namespace Aura::UI::Mixer {

/**
 * @class ChannelStrip
 * @brief High-Fidelity Professional Mixer Component.
 * Logic Pro 11 parity: Aluminum Faders, Neon Meters, and Hardware-Modeled Slots.
 */
class ChannelStrip {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const Core::Engine::Track& track) {
        // --- 1. EBONY FRAME ---
        kernel.drawGradientRect(x, y, w, h, 0xFF1C1C1E, 0xFF141416);
        kernel.drawLine(x + w - 1, y, x + w - 1, y + h, 1.2f, 0xFF000000);
        
        float cx = x + 10, cw = w - 20;

        // --- 2. EFFECT SLOTS (Logic-Azure and Gold) ---
        float slotY = y + 10;
        for (int i = 0; i < 6; ++i) {
            bool hasFx = i < (int)track.getEffects().size();
            uint32_t slotCol = hasFx ? (i > 2 ? 0xFF9F722D : 0xFF2E6DA4) : 0xFF0D0D0F;
            
            kernel.drawRoundedRect(cx, slotY + i*22, cw, 20, 3.0f, slotCol);
            if (hasFx) {
               std::string name = track.getEffects()[i]->getName();
               kernel.drawText(name.substr(0, 14), cx + 8, slotY + i*22 + 14, 8, 0xFFF1F5F9);
            }
        }

        // --- 3. PAN DIAL: Hardware Texture ---
        float panY = slotY + 160.0f;
        kernel.drawCircle(x + w/2, panY, 14, 0xFF0A0A0C);
        kernel.drawBrushedCircle(x + w/2, panY, 12, 0xFF4B4B4E);
        
        float panVal = track.getPan(); // -1.0 to 1.0
        float angle = panVal * (135.0f * (3.14159f / 180.0f));
        kernel.drawLine(x + w/2, panY, x + w/2 + std::sin(angle)*10, panY - std::cos(angle)*10, 2.5f, 0xFFFFFFFF);
        kernel.drawArc(x + w/2, panY, 15, -135, -135 + (panVal + 1.0)*135, 2.0f, 0xFF30B0FF);

        // --- 4. PRECISE VOLUME FADER & METER ---
        float fAreaY = panY + 30.0f;
        float fAreaH = h - (fAreaY - y) - 45.0f;
        
        // Fader Groove
        kernel.drawRect(x + 24, fAreaY, 4, fAreaH, 0xFF050505);
        kernel.drawRect(x + 25, fAreaY, 2, fAreaH, 0x44FFFFFF);

        // Peak Meter (Logic-Neon)
        float meterW = 12.0f;
        kernel.drawMeter(track.getId(), track.getPeakL(), track.getPeakR(), x + w - meterW - 10, fAreaY, meterW, fAreaH);

        // SKEUOMORPHIC FADER CAP
        float vol = track.getVolume();
        float capY = fAreaY + fAreaH * (1.0f - std::clamp(vol, 0.0f, 1.0f));
        float capW = 34, capH = 28;
        float capX = x + 16 - 8;
        
        kernel.drawDropShadow(capX, capY - capH/2 + 2, capW, capH, 5, 0xAA000000);
        kernel.drawGradientRect(capX, capY - capH/2, capW, capH, 0xFFE5E7EB, 0xFF9CA3AF);
        kernel.drawRect(capX + 4, capY - 1, capW - 8, 2, 0xFF30B0FF); // Indicator
        
        // Numerical DB
        char dbStr[16]; snprintf(dbStr, sizeof(dbStr), "%.1f", 20.0f * std::log10(vol + 0.0001f));
        kernel.drawText(dbStr, x + 6, fAreaY + fAreaH + 12, 8, 0xFF94A3B8);

        // --- 5. FOOTER: Track Branding ---
        kernel.drawRect(x, y + h - 30, w, 30, 0xFF111113);
        kernel.drawRect(x, y + h - 30, w, 4, track.getColor());
        kernel.drawText(track.getName(), x + 8, y + h - 10, 10, 0xFFF1F5F9);
    }
};

} // namespace Aura::UI::Mixer
