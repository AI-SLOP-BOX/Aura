#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @brief FlexTimeEngine: Professional granular time-stretching.
 * Logic Pro-style "Flex Time" that changes length without affecting pitch.
 * Uses a basic Overlap-Add (OLA) strategy for real-time safety.
 */
class FlexTimeEngine {
public:
    explicit FlexTimeEngine(double sr) : m_sampleRate(sr) {}

    /**
     * @brief Stretches a source buffer by a specific factor.
     * @param factor: 2.0 means twice the length (slow), 0.5 means half (fast).
     */
    std::vector<float> process(const std::vector<float>& source, float factor) {
        if (std::abs(factor - 1.0f) < 0.01f) return source;

        size_t targetSize = static_cast<size_t>(source.size() * factor);
        std::vector<float> output(targetSize, 0.0f);

        const size_t grainSize = 1024;
        const size_t hopSize = 512;
        size_t readPos = 0;
        size_t writePos = 0;

        while (readPos + grainSize < source.size() && writePos + grainSize < targetSize) {
            // Apply a simple Hann window to overlap-add grains
            for (size_t i = 0; i < grainSize; ++i) {
                float window = 0.5f * (1.0f - std::cos(2.0f * 3.14159f * i / (grainSize - 1)));
                output[writePos + i] += source[readPos + i] * window;
            }
            
            readPos += static_cast<size_t>(hopSize / factor);
            writePos += hopSize;
        }

        return output;
    }

private:
    double m_sampleRate;
};

} // namespace Aura::DSP::Analysis
