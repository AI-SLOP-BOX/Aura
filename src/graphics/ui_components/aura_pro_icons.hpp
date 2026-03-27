#pragma once
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class ProfessionalIcons
 * @brief Logic Pro 11 Style High-Fidelity Professional SDF Icons.
 * Maps specific glyph codes (ASCII) to the Metal SDF Procedural Engine.
 */
class ProfessionalIcons {
public:
    static void drawSolo(::Aura::Graphics::Platform::IGraphicsKernel& k, float x, float y, float s, bool active) {
        uint32_t col = active ? 0xFFEAB308 : 0xFFFFFFFF; // Logic Yellow Solo
        k.drawRoundedRect(x, y, s, s, 3.0f, active ? 0xFF422006 : 0xFF2A2A2E);
        k.drawNative(x + s*0.2f, y + s*0.2f, s*0.6f, s*0.6f, 83.0f, 0, 0, col, col, 4.0); // 'S' Glyp
    }

    static void drawMute(::Aura::Graphics::Platform::IGraphicsKernel& k, float x, float y, float s, bool active) {
        uint32_t col = active ? 0xFFEF4444 : 0xFFFFFFFF; // Logic Red Mute
        k.drawRoundedRect(x, y, s, s, 3.0f, active ? 0xFF450A0A : 0xFF2A2A2E);
        k.drawNative(x + s*0.2f, y + s*0.2f, s*0.6f, s*0.6f, 77.0f, 0, 0, col, col, 4.0); // 'M' Glyp
    }

    static void drawRecord(::Aura::Graphics::Platform::IGraphicsKernel& k, float x, float y, float s, bool active) {
        uint32_t col = active ? 0xFFFF0002 : 0xFF4B5563;
        k.drawRoundedRect(x, y, s, s, 3.0f, 0xFF1C1C1E);
        k.drawNative(x + s*0.3f, y + s*0.3f, s*0.4f, s*0.4f, 8.0f, 0, 0, col, col, 4.0); // Record Dot
    }

    static void drawPower(::Aura::Graphics::Platform::IGraphicsKernel& k, float x, float y, float s, bool active) {
        uint32_t col = active ? 0xFF30B0FF : 0xFF4B5563;
        k.drawRoundedRect(x, y, s, s, 4.0f, 0xFF1C1C1E);
        k.drawCircle(x + s/2, y + s/2, s*0.2f, col);
    }

    static void drawLock(::Aura::Graphics::Platform::IGraphicsKernel& k, float x, float y, float s, bool active) {
        uint32_t col = active ? 0xFF30B0FF : 0xFF94A3B8;
        k.drawRoundedRect(x + s*0.2f, y + s*0.45f, s*0.6f, s*0.45f, 2.0f, col);
        k.drawCircle(x+s/2, y+s*0.35f, s*0.2f, col);
    }
};

} // namespace Aura::Graphics::UI
