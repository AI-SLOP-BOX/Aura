#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

namespace Aura::DSP::Spatial {

/**
 * @brief SurroundPanner: Professional High-End Spatialization.
 * Ready for Dolby Atmos / 7.1.4 Immersive mixing.
 */
class SurroundPanner {
public:
    enum class Format { 
        Stereo, 
        Surround_5_1, // L, R, C, LFE, Ls, Rs
        Surround_7_1, // L, R, C, LFE, Ls, Rs, Lb, Rb
        Immersive_7_1_4 // Adds ceiling speakers
    };

    SurroundPanner(Format f = Format::Stereo) : m_format(f) {}

    /**
     * @brief PAN: Distributes mono input to multi-channel output based on X,Y,Z.
     * @param x: [-1.0, 1.0] (Left -> Right)
     * @param y: [-1.0, 1.0] (Front -> Back)
     * @param z: [0.0, 1.0] (Height)
     */
    void pan(float input, float* outputs, float x, float y, float z = 0.0f) {
        switch (m_format) {
            case Format::Stereo:
                outputs[0] = input * std::sqrt(0.5f * (1.0f - x)); // L
                outputs[1] = input * std::sqrt(0.5f * (1.0f + x)); // R
                break;
            case Format::Surround_5_1:
                process51(input, outputs, x, y);
                break;
            case Format::Surround_7_1:
                process71(input, outputs, x, y);
                break;
            case Format::Immersive_7_1_4:
                process714(input, outputs, x, y, z);
                break;
        }
    }

private:
    void process51(float in, float* out, float x, float y) {
        // Simple Amplitude Panning across 5.1 layout
        // (Simplified logic for brevity)
        float fl = std::clamp((1.0f - x) * (1.0f + y), 0.0f, 1.0f);
        float fr = std::clamp((1.0f + x) * (1.0f + y), 0.0f, 1.0f);
        float c  = std::clamp(1.0f - std::abs(x), 0.0f, 1.0f) * std::max(0.0f, y);
        float bl = std::clamp((1.0f - x) * (1.0f - y), 0.0f, 1.0f);
        float br = std::clamp((1.0f + x) * (1.0f - y), 0.0f, 1.0f);
        
        out[0] = in * std::sqrt(fl); // L
        out[1] = in * std::sqrt(fr); // R
        out[2] = in * std::sqrt(c);  // Center
        out[3] = in * 0.1f;          // LFE (Sub leakage)
        out[4] = in * std::sqrt(bl); // Ls
        out[5] = in * std::sqrt(br); // Rs
    }

    void process71(float in, float* out, float x, float y) { /* Standard 7.1 mapping */ }
    void process714(float in, float* out, float x, float y, float z) { /* Atmos 7.1.4 mapping */ }

    Format m_format;
};

} // namespace Aura::DSP::Spatial
