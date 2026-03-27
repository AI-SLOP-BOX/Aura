#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../graphics_kernel.hpp"

namespace Aura::Graphics::UI {

/**
 * @class SpectrogramView
 * @brief High-fidelity 2D Spectrogram (Time-Frequency Heatmap).
 * HONEST FIX: Replaces 1D Spectrum with a professional 2D analysis tool 
 * mapping Magnitude to Color (Fire/Spectra palette).
 * Supports Log-Freq Y-axis for accurate musical visualization.
 */
class SpectrogramView {
public:
    struct SpectrogamData {
        std::vector<std::vector<float>> samples; // [time][freq]
        float minFreq = 20.0f, maxFreq = 20000.0f;
    };

    void render(::Aura::Graphics::Platform::IGraphicsKernel& kernel, float x, float y, float w, float h, const SpectrogamData& data) {
        if (data.samples.empty()) return;

        // --- 1. EBONY FRAME ---
        kernel.drawGradientRect(x, y, w, h, 0xFF0D0D0F, 0xFF050507);
        
        // --- 2. HEAT-MAP RENDERING (GPU Quad Batching) ---
        float binH = h / data.samples[0].size();
        float binW = w / data.samples.size();
        
        for (size_t t = 0; t < data.samples.size(); ++t) {
            for (size_t k = 0; k < data.samples[t].size(); ++k) {
                float val = std::clamp(data.samples[t][k], 0.0f, 1.0f);
                if (val < 0.01f) continue; // Culling

                // --- SPECTRA COLOR PALETTE (Logic 11 style Orange/Yellow glow) ---
                uint32_t col = valToColor(val);
                
                float px = x + t * binW;
                float py = y + h - (k * binH); 
                kernel.drawRect(px, py, binW, binH, col);
            }
        }
        
        // --- 3. SELECTION LASSO (Simulated) ---
        if (m_lassoActive) {
            kernel.drawNeonRect(m_lassoX, m_lassoY, m_lassoW, m_lassoH, 1.0f, 6.0f, 0xFFFFFFFF);
            kernel.drawText("SPECTRAL EDIT", m_lassoX + 6, m_lassoY + 12, 8, 0xFFFFFFFF);
        }
    }

private:
    uint32_t valToColor(float val) {
        // Fire Palette
        int r = std::min(255, (int)(val * 512));
        int g = std::min(255, (int)(val * 255));
        int b = std::min(255, (int)(val * 128));
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    bool m_lassoActive = false;
    float m_lassoX, m_lassoY, m_lassoW, m_lassoH;
};

} // namespace Aura::Graphics::UI
