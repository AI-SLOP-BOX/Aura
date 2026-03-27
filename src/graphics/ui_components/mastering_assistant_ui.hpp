#pragma once
#include "ui_view.hpp"
#include "../../scae/AuraAISuite.hpp"
#include "../../dsp/mixing/master_suite.hpp"

namespace Aura::Graphics::UI {

/**
 * @class MasteringAssistantUI
 * @brief Logic Pro 11 style AI Mastering Assistant.
 * Provides Spectral Balancing, Dynamics matching, and Stereo Width optimization.
 */
class MasteringAssistantUI : public View {
public:
    enum class Character { Clean, Modern, Vintage, Punchy };

    MasteringAssistantUI() {
        // Initialize with default target
        m_targetLufs = -14.0f;
        m_char = Character::Modern;
    }

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel) override {
        if (!m_visible) return;
        auto b = m_bounds;

        // --- 1. PREMIUM GLASS BACKDROP ---
        kernel.applyBlurEffect(b.x, b.y, b.w, b.h, 40.0f);
        kernel.drawGradientRect(b.x, b.y, b.w, b.h, 0xCC111115, 0xEE09090D);
        kernel.drawRoundedRect(b.x, b.y, b.w, b.h, 0, 0x22FFFFFF); // Fine border

        // --- 2. HEADER: AI MASTERING ASSISTANT ---
        kernel.drawText("AI MASTERING ASSISTANT", b.x + 40, b.y + 50, 24, 0xFF30B0FF);
        kernel.drawText("PROJECT ANALYSIS & TONAL MATCHING", b.x + 40, b.y + 75, 10, 0xFF6B7280);

        // --- 3. CHARACTER SELECTOR (Logic style segmented control) ---
        float charX = b.x + 40;
        float charY = b.y + 110;
        renderCharacterSelector(kernel, charX, charY);

        // --- 4. SPECTRAL BALANCE CURVE (Neural Analysis) ---
        float graphX = b.x + 40;
        float graphY = b.y + 180;
        float graphW = b.w - 80;
        float graphH = 200;
        renderSpectralGraph(kernel, graphX, graphY, graphW, graphH);

        // --- 5. CONTROLS: LOUDNESS & WIDTH ---
        float ctrlX = b.x + 40;
        float ctrlY = graphY + graphH + 40;
        renderFader(kernel, ctrlX, ctrlY, 300, "LOUDNESS (LUFS)", m_targetLufs, -24, -8);
        renderFader(kernel, ctrlX + 400, ctrlY, 300, "STEREO WIDTH", m_width, 0, 2);

        // --- 6. NEURAL MASTERING ACTION BUTTONS ---
        float btnW = 220;
        float btnX = b.x + b.w - btnW - 40;
        float btnY = b.y + b.h - 80;
        
        if (m_isAnalyzing) {
            kernel.drawRoundedRect(btnX, btnY, btnW, 40, 6, 0xFF1F2937);
            kernel.drawText("ANALYZING...", btnX + 65, btnY + 25, 12, 0xFFFFFFFF);
            // Progress bar
            kernel.drawRect(btnX, btnY + 38, btnW * m_progress, 2, 0xFF30B0FF);
        } else {
            bool hover = false; // Mock hover
            kernel.drawRoundedRect(btnX, btnY, btnW, 40, 6, hover ? 0xFF3B82F6 : 0xFF2563EB);
            kernel.drawText("RE-ANALYZE PROJECT", btnX + 40, btnY + 25, 12, 0xFFFFFFFF);
        }
        
        // --- 7. APPLIED STATUS ---
        if (m_isApplied) {
            kernel.drawText("NEURAL PROFILE APPLIED TO MASTER BUS", b.x + 40, b.y + b.h - 40, 10, 0xFF34C759);
        }
    }

private:
    void renderCharacterSelector(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y) {
        std::vector<std::string> labels = {"CLEAN", "MODERN", "VINTAGE", "PUNCHY"};
        float segmentW = 100;
        for (int i = 0; i < 4; ++i) {
            bool active = (m_char == (Character)i);
            kernel.drawRoundedRect(x + i * segmentW, y, segmentW, 30, 4, active ? 0xFF30B0FF : 0xFF1F2937);
            kernel.drawText(labels[i], x + i * segmentW + 25, y + 20, 9, active ? 0xFF000000 : 0xFF9CA3AF);
        }
    }

    void renderSpectralGraph(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h) {
        // Grid
        kernel.drawRect(x, y, w, h, 0xFF111827);
        kernel.drawLine(x, y + h*0.5f, x + w, y + h*0.5f, 1.0f, 0x33FFFFFF); // Zero line

        // Mock spectral curve (NeuralCorrection)
        // Draw a smooth Bezier or Polyline that matches the Logic 11 analysis curve
        std::vector<float> points;
        for (int i = 0; i < 100; ++i) {
            float val = std::sin(i * 0.1f) * 20.0f + std::cos(i * 0.05f) * 10.0f;
            if (m_char == Character::Vintage) val += 15.0f * std::exp(-(i-20)*(i-20)/100.0f); // Bass boost
            points.push_back(val);
        }
        
        // Draw the curve with a cyan glow
        for (int i = 0; i < 99; ++i) {
            float px1 = x + (i * w / 100.0f);
            float py1 = y + h*0.5f - points[i];
            float px2 = x + ((i+1) * w / 100.0f);
            float py2 = y + h*0.5f - points[i+1];
            kernel.drawLine(px1, py1, px2, py2, 2.0f, 0xFF30B0FF);
        }
    }

    void renderFader(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, const std::string& label, float val, float min, float max) {
        kernel.drawText(label, x, y - 10, 10, 0xFF9CA3AF);
        kernel.drawRect(x, y, w, 4, 0xFF1F2937);
        float progress = (val - min) / (max - min);
        kernel.drawRect(x, y, w * progress, 4, 0xFF30B0FF);
        kernel.drawCircle(x + w * progress, y + 2, 8, 0xFFFFFFFF);
    }

    Character m_char = Character::Modern;
    float m_targetLufs = -14.0f;
    float m_width = 1.0f;
    bool m_isAnalyzing = false;
    float m_progress = 0.0f;
    bool m_isApplied = true;
};

} // namespace Aura::Graphics::UI
