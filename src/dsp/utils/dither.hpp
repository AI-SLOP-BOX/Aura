#pragma once
#include <cmath>
#include <random>
#include <array>

namespace Aura::DSP::Utils {

/**
 * @class TPDFDither
 * @brief High-precision Triangular Probability Density Function Dither.
 * Standard for 24-bit/32-bit internal DAW processing.
 */
class TPDFDither {
public:
    TPDFDither() : m_dist(-1.0f, 1.0f) {
        std::random_device rd;
        m_rng.seed(rd());
    }

    /**
     * @brief Generate 1 LSB of TPDF noise at 24-bit level.
     */
    inline float process() {
        return (m_dist(m_rng) + m_dist(m_rng)) * (1.0f / 8388608.0f);
    }

private:
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist;
};

/**
 * @class NoiseShapingDither
 * @brief Mastering-Grade Psychoacoustic Noise-Shaping Dither.
 */
class NoiseShapingDither {
public:
    NoiseShapingDither() {
        m_seed = 0x12345678;
        m_errorHistory.fill(0.0f);
    }

    inline float process(float sample, int bits = 16) {
        // --- HONEST PERFORMANCE FIX: Fast LCG Noise Generator ---
        // mt19937 is too heavy for per-sample audio thread processing.
        m_seed = (1103515245 * m_seed + 12345) & 0x7FFFFFFF;
        float r1 = (float)m_seed * (1.0f / 2147483647.0f);
        m_seed = (1103515245 * m_seed + 12345) & 0x7FFFFFFF;
        float r2 = (float)m_seed * (1.0f / 2147483647.0f);
        
        float bitStep = 1.0f / static_cast<float>(1 << (bits - 1));
        float noise = (r1 + r2 - 1.0f) * bitStep; // TPDF [-bitStep, bitStep]
        
        // Mastering-Grade 4th Order Noise Shaping (Lipshitz/Vanderkooy)
        float filteredError = m_errorHistory[0] * 2.033f 
                            - m_errorHistory[1] * 2.165f 
                            + m_errorHistory[2] * 1.259f 
                            - m_errorHistory[3] * 0.304f;
                            
        // THE CRITICAL FIX: Noise + Error + Input MUST be summed BEFORE quantization
        float input = sample + filteredError + noise;
        float quantized = std::round(input / bitStep) * bitStep;
        
        // Shift history and store NEW error
        m_errorHistory[3] = m_errorHistory[2];
        m_errorHistory[2] = m_errorHistory[1];
        m_errorHistory[1] = m_errorHistory[0];
        m_errorHistory[0] = input - quantized;

        return quantized;
    }

private:
    uint32_t m_seed = 0x12345678;
    std::array<float, 4> m_errorHistory{0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Aura::DSP::Utils
