#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "../graphics_kernel.hpp"
#include "../../dsp/analysis/spectrum_analyzer.hpp"

namespace Aura::Graphics::UI {

/**
 * @class SpectrumRenderer
 * @brief High-performance FFT Spectrum Visualizer.
 * Logic Pro Style Gradient Fill + Peak Line.
 */
class SpectrumRenderer {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const std::vector<float>& bands, const std::vector<float>& aiTarget = {}) {
        if (bands.empty()) return;
        m_history.push_back(bands);
        if (m_history.size() > 40) m_history.erase(m_history.begin());

        // --- 1. EBONY ENCLOSURE ---
        kernel.drawGradientRect(x, y, w, h, 0xFF0A0A0C, 0xFF141416);
        
        // --- 2. 3D WATERFALL (Premium Perspective Rendering) ---
        float depthScale = 0.8f;
        float hOffset = h * 0.4f;
        for (int i = (int)m_history.size() - 1; i >= 0; --i) {
            float z = (float)i / m_history.size();
            float sliceX = x + (1.0f - z) * 20.0f;
            float sliceY = y + (1.0f - z) * hOffset;
            float sliceW = w * (0.8f + 0.2f * z);
            float sliceH = h * 0.5f;
            
            uint32_t col = (0x88 << 24) | (0x22 << 16) | (0xD3 << 8) | ( (int)(0xEE * z) ); // Gradual shift
            drawSlice(kernel, sliceX, sliceY, sliceW, sliceH, m_history[i], col);
        }

        // --- 3. AI TARGET CURVE (Smart EQ Overlay) ---
        if (!aiTarget.empty()) {
            float bandW = w / (float)aiTarget.size();
            for (size_t i = 0; i < aiTarget.size() - 1; ++i) {
                float bx1 = x + i * bandW;
                float by1 = y + h/2 - aiTarget[i] * 5.0f; // Scale correction dB to pixels
                float bx2 = x + (i + 1) * bandW;
                float by2 = y + h/2 - aiTarget[i+1] * 5.0f;
                kernel.drawLine(bx1, by1, bx2, by2, 2.0f, 0xFFFDE047); // Yellow Target
                if (i % 20 == 0) kernel.drawCircle(bx1, by1, 2, 0xFFFDE047);
            }
            kernel.drawText("AI SMART EQ TARGET", x + 10, y + 25, 8, 0xFFFDE047);
        }
        
        // --- 4. FREQUENCY LABELS (Mock 20Hz - 20kHz) ---
        kernel.drawText("20", x + 5, y + h - 15, 7, 0xFF666666);
        kernel.drawText("1k", x + w/2 - 10, y + h - 15, 7, 0xFF666666);
        kernel.drawText("20k", x + w - 25, y + h - 15, 7, 0xFF666666);
    }

private:
    void drawSlice(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const std::vector<float>& bands, uint32_t color) {
        float bandW = w / (float)bands.size();
        for (size_t i = 0; i < bands.size() - 1; ++i) {
            float bx1 = x + i * bandW;
            float by1 = y + h - std::min(h, bands[i] * h * 5.0f);
            float bx2 = x + (i + 1) * bandW;
            float by2 = y + h - std::min(h, bands[i+1] * h * 5.0f);
            kernel.drawLine(bx1, by1, bx2, by2, 1.5f, color);
        }
    }

    std::vector<std::vector<float>> m_history;
};

} // namespace Aura::Graphics::UI
