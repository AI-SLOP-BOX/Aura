#pragma once
#include <vector>
#include <cmath>
#include "../graphics_kernel.hpp"
#include "../../dsp/analysis/spectrum_analyzer.hpp"

namespace Aura::Graphics::UI {

/**
 * @class SpectrumView
 * @brief High-fidelity Real-time Spectrum Analyzer Display.
 * Renders a smooth interpolated curve with an 'outer glow' effect 
 * using GPU vertex batching. Logic Pro X Pro-EQ style.
 */
class SpectrumView {
public:
    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const std::vector<float>& bands) {
        if (bands.empty()) return;

        // --- 1. BACKGROUND GRID (Logarithmic Freq) ---
        kernel.drawGradientRect(x, y, w, h, 0xFF0D0D0F, 0xFF050507);
        
        float log20 = std::log10(20.0f);
        float log20k = std::log10(20000.0f);
        std::vector<float> gridFreqs = { 100, 1000, 10000 };
        for (float f : gridFreqs) {
            float normX = (std::log10(f) - log20) / (log20k - log20);
            float gx = x + normX * w;
            kernel.drawLine(gx, y, gx, y + h, 1.0f, 0x11FFFFFF);
            kernel.drawText(std::to_string((int)f) + "Hz", gx + 2, y + h - 12, 8, 0xFF444444);
        }

        // --- 2. ANALYZER CURVE (Smooth Gradient Path) ---
        std::vector<Vertex> path;
        path.reserve(bands.size());
        
        float stepX = w / (bands.size() - 1);
        for (size_t i = 0; i < bands.size(); ++i) {
            float px = x + i * stepX;
            // Map magnitude (0..1) to height with logarithmic sensitivity (dB-like)
            float mag = bands[i];
            float db = 20.0f * std::log10(mag + 1e-6f);
            float normY = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
            float py = y + h - (normY * h * 0.9f) - 10.0f;
            
            // Glow effect color (Neon Cyan)
            path.push_back({px, py, 0xFF22D3EE});
        }

        // Draw the curve with thickness and glow
        kernel.drawVertexPath(path, 2.0f);

        // --- 3. FILL AREA (Semi-transparent gradient fill) ---
        std::vector<float> fillPoints;
        fillPoints.reserve(path.size() * 2 + 4);
        for (const auto& v : path) {
            fillPoints.push_back(v.x);
            fillPoints.push_back(v.y);
        }
        fillPoints.push_back(x + w); fillPoints.push_back(y + h);
        fillPoints.push_back(x); fillPoints.push_back(y + h);
        
        kernel.drawFilledPath(fillPoints, 0x3322D3EE);
    }
};

} // namespace Aura::Graphics::UI
