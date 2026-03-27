#pragma once

#include <cmath>
#include <algorithm>
#include "../math/fast_math.hpp"
#include "oversampler.hpp"
#include "../iprocessor.hpp"

namespace Aura::DSP::Effects {

/**
 * @brief AnalogSaturator: High-fidelity Harmonic Exciter and Soft Clipper.
 */
class AnalogSaturator : public IProcessor {
public:
    enum class Model { Tube, Tape, SoftClip };

    AnalogSaturator(double sr = 44100.0) : m_sampleRate(sr) {}

    void process(float* l, float* r, uint32_t numSamples) override {
        // Default processing with neutral settings if not configured
        processWithSettings(l, r, numSamples, 0.2f, 0.5f, Model::Tube);
    }

    /**
     * @brief Processes a block of samples with specific settings.
     */
    void processWithSettings(float* l, float* r, uint32_t numSamples, float drive, float warmth, Model model = Model::Tube) {
        float driveLin = m_math.dbToLinear(drive * 24.0f);
        float comp = 1.0f / (1.0f + drive * 0.7f); 

        for (uint32_t i = 0; i < numSamples; ++i) {
            float inL = l[i] * driveLin;
            float inR = r[i] * driveLin;
            
            float Ly1, Ly2;
            m_oversamplerL.upsample(inL, Ly1, Ly2);
            l[i] = m_oversamplerL.downsample(applyModel(Ly1, warmth, model), applyModel(Ly2, warmth, model)) * comp;

            float Ry1, Ry2;
            m_oversamplerR.upsample(inR, Ry1, Ry2);
            r[i] = m_oversamplerR.downsample(applyModel(Ry1, warmth, model), applyModel(Ry2, warmth, model)) * comp;

            m_dcL = 0.999f * m_dcL + 0.001f * l[i]; l[i] -= m_dcL;
            m_dcR = 0.999f * m_dcR + 0.001f * r[i]; r[i] -= m_dcR;
        }
    }

    void setSampleRate(double sr) override { m_sampleRate = sr; }
    uint32_t getLatency() const override { return 0; }

private:
    inline float applyModel(float x, float warmth, Model model) {
        switch (model) {
            case Model::Tube: {
                float b = warmth * 0.25f; // Bias
                return (x + b) / (1.0f + std::abs(x + b)) - (b / (1.0f + std::abs(b)));
            }
            case Model::Tape: {
                float xAbs = std::abs(x);
                if (xAbs < 1.0f) return x * (1.5f - 0.5f * x * x);
                return (x > 0 ? 1.0f : -1.0f);
            }
            case Model::SoftClip: return std::tanh(x);
        }
        return x;
    }

    double m_sampleRate;
    float m_dcL = 0.0f, m_dcR = 0.0f;
    Oversampler2x m_oversamplerL, m_oversamplerR;
    Math::FastMath m_math;
};

} // namespace Aura::DSP::Effects
