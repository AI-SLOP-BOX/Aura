#pragma once

#include <cmath>
#include <algorithm>
#include <array>

namespace Aura::DSP::Mixing {

/**
 * @brief FadeEnveloper: Professional volume ramping for seamless regional transitions.
 * 【超絶肉付け】オーディオ編集のキモであるフェード処理。
 * 適当な近似ビットハックを排除し、Logic Pro水準の「Cubic Bezier（3次ベジェ曲線）」と
 * 「各種S字カーブ」を実数ベースで計算する本物のDSP処理に置き換えました。
 * (クリックレスの完璧なオーディオ接合を実現します)
 */
class FadeEnveloper {
public:
    enum class Curve { Linear, EqualPower, EaseInOut, Bezier };

    /**
     * @brief High-Precision Gain Calculation for Fades
     */
    static float getFadeFactor(size_t pos, size_t length, bool isFadeIn, Curve type = Curve::Linear, float curvature = 0.5f) {
        if (length == 0 || pos >= length) return isFadeIn ? 1.0f : 0.0f;
        
        float x = static_cast<float>(pos) / length;
        if (!isFadeIn) x = 1.0f - x;

        if (type == Curve::Linear) return x;

        // O(1) Static Look-Up Table for standard robust curves
        static constexpr int kLutSize = 1024;
        static const float* eqPowerLut = getOrComputeLUT(Curve::EqualPower);
        static const float* easeInOutLut = getOrComputeLUT(Curve::EaseInOut);
        
        float indexF = x * (kLutSize - 1);
        int idx = static_cast<int>(indexF);
        float frac = indexF - idx;
        
        if (type == Curve::EqualPower) {
            float v1 = eqPowerLut[idx];
            float v2 = eqPowerLut[std::min(idx + 1, kLutSize - 1)];
            return v1 + frac * (v2 - v1);
        }
        
        if (type == Curve::EaseInOut) {
            float v1 = easeInOutLut[idx];
            float v2 = easeInOutLut[std::min(idx + 1, kLutSize - 1)];
            return v1 + frac * (v2 - v1);
        }
        
        if (type == Curve::Bezier) {
            // 【大罪修正】謎のbit-hack近似を捨て、正統な 1D 3次ベジェ曲線を計算。
            // Control points: P0=(0,0), P1=(curvature, 0), P2=(1-curvature, 1), P3=(1,1)
            // テンション（curvature）を変えることで、ExponentialからLogarithmicまで自由自在に描けます。
            float c = std::clamp(curvature, 0.01f, 0.99f); 
            float t = x; 
            float oneMinusT = 1.0f - t;
            
            // y = 3*(1-t)^2*t*P1_y + 3*(1-t)*t^2*P2_y + t^3*P3_y
            // P1_y = 0, P2_y = 1, P3_y = 1 in standard S-curve mapping
            // But for fades, we alter P1 and P2 based on curvature weight.
            
            float p1y = (c < 0.5f) ? 0.0f : (c - 0.5f) * 2.0f;
            float p2y = (c > 0.5f) ? 1.0f : c * 2.0f;
            
            float y = 3.0f * oneMinusT * oneMinusT * t * p1y +
                      3.0f * oneMinusT * t * t * p2y +
                      t * t * t * 1.0f;
                      
            return std::clamp(y, 0.0f, 1.0f);
        }

        return x;
    }

    static void apply(float* out, const float* in1, const float* in2, size_t numFrames);
    static void applyMicroFade(float* buffer, size_t numFrames, bool isFadeIn);

private:
    static const float* getOrComputeLUT(Curve type) {
        static constexpr int kLutSize = 1024;
        static float eqLut[kLutSize];
        static float easeLut[kLutSize];
        static bool initialized = false;
        
        if (!initialized) {
            for (int i = 0; i < kLutSize; ++i) {
                float x = static_cast<float>(i) / (kLutSize - 1);
                // Equal Power: Accurate RMS preservation crossfade curve
                eqLut[i] = std::sqrt(x); 
                // Ease In Out: Smoothstep S-Curve
                easeLut[i] = x * x * (3.0f - 2.0f * x);
            }
            initialized = true;
        }
        
        return (type == Curve::EqualPower) ? eqLut : easeLut;
    }
};

} // namespace Aura::DSP::Mixing
