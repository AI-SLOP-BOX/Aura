#pragma once

#include <vector>
#include <cmath>
#include <numbers>

namespace Aura::DSP::Analysis {

/**
 * @brief AudioResampler: High-quality Sample Rate Converter (SRC).
 * Ensures audio consistency when importing assets with different sample rates.
 * Uses Cubic Hermite Spline for production-grade sonic fidelity.
 */
class AudioResampler {
public:
    /**
     * @brief Resamples a source buffer from one rate to another.
     */
    static std::vector<float> process(const std::vector<float>& source, double sourceRate, double targetRate) {
        if (std::abs(sourceRate - targetRate) < 0.1) return source; // No conversion needed

        double ratio = sourceRate / targetRate;
        size_t targetSize = static_cast<size_t>(source.size() / ratio);
        std::vector<float> output;
        output.reserve(targetSize);

        for (size_t i = 0; i < targetSize; ++i) {
            double sourcePos = i * ratio;
            output.push_back(interpolateCubic(source, sourcePos));
        }
        return output;
    }

private:
    /**
     * @brief 4-Point Cubic Hermite Interpolation for low aliasing and high clarity.
     */
    static float interpolateCubic(const std::vector<float>& buffer, double pos) {
        int i1 = static_cast<int>(pos);
        int i0 = (i1 > 0) ? i1 - 1 : 0;
        int i2 = (i1 < (int)buffer.size() - 1) ? i1 + 1 : (int)buffer.size() - 1;
        int i3 = (i2 < (int)buffer.size() - 1) ? i2 + 1 : i2;

        float f = static_cast<float>(pos - i1);
        float a = (-0.5f * buffer[i0] + 1.5f * buffer[i1] - 1.5f * buffer[i2] + 0.5f * buffer[i3]);
        float b = (buffer[i0] - 2.5f * buffer[i1] + 2.0f * buffer[i2] - 0.5f * buffer[i3]);
        float c = (-0.5f * buffer[i0] + 0.5f * buffer[i2]);
        float d = buffer[i1];

        return a * f * f * f + b * f * f + c * f + d;
    }
};

} // namespace Aura::DSP::Analysis
