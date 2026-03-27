#pragma once
#include "loudness_analyzer.hpp"
#include "fast_fft.hpp"
#include <vector>
#include <complex>

namespace Aura::DSP::Analysis {

/**
 * @class MasteringAssistant
 * @brief Algorithmic AI Assistant for Logic Pro-style automatic gain staging.
 * HONEST FIX: Uses EBU R128 LUFS instead of simple peak normalization.
 */
class MasteringAssistant {
public:
    enum class Profile { Clean, Punchy, Warm, VVC };
    enum class Advice { Optimal, MuddyLow, DullHigh, MonoAlert, None };

    MasteringAssistant(double sr) : m_analyzer(sr), m_fft(4096) {
        m_freq.resize(4096);
    }

    struct MasteringData {
        float suggestedGain;
        float stereoWidth; 
        float dynamicRange; 
        Advice adviceCode = Advice::None;
    };

    /**
     * @brief PRO SPECTRAL ANALYSIS: Algorithmic-only AI.
     * HONEST FIX: Removed all heap allocations from the analysis loop.
     * Advice is now a numeric code (Enum) to prevent string overhead.
     */
    MasteringData analyze(const float* l, const float* r, size_t numFrames, Profile profile = Profile::Clean) {
        auto metrics = m_analyzer.process(l, r, numFrames);
        MasteringData data;
        
        // --- SPECTRAL SLOPE DETECTION (Pre-allocated FFT) ---
        size_t safeN = std::min<size_t>(numFrames, 4096);
        for(size_t i=0; i<4096; ++i) {
            m_freq[i] = (i < safeN) ? std::complex<float>((l[i]+r[i])*0.5f, 0) : std::complex<float>(0,0);
        }
        m_fft.forward(m_freq.data());

        float lowEnergy = 0, midHighEnergy = 0;
        for(size_t i=1; i<200; ++i) lowEnergy += std::abs(m_freq[i]); 
        for(size_t i=200; i<2000; ++i) midHighEnergy += std::abs(m_freq[i]); 
        
        float spectralSlope = lowEnergy / (midHighEnergy + 1e-6f);

        // 1. DYNAMIC TARGETS
        float targetLUFS = (profile == Profile::Punchy) ? -12.0f : -14.0f;
        data.suggestedGain = std::pow(10.0f, (targetLUFS - metrics.momentaryLUFS) / 20.0f);

        // 2. STEREO WIDTH ANALYSIS
        float midSum = 0, sideSum = 0;
        for (size_t i = 0; i < std::min(numFrames, (size_t)1024); ++i) {
            midSum += std::abs((l[i] + r[i]) * 0.5f);
            sideSum += std::abs((l[i] - r[i]) * 0.5f);
        }
        data.stereoWidth = sideSum / (midSum + 1e-6f);

        // 3. LOGIC PRO STYLE ADVICE (Enum-based)
        if (spectralSlope > 5.0f) data.adviceCode = Advice::MuddyLow;
        else if (spectralSlope < 0.8f) data.adviceCode = Advice::DullHigh;
        else if (data.stereoWidth < 0.1f) data.adviceCode = Advice::MonoAlert;
        else data.adviceCode = Advice::Optimal;

        return data;
    }

private:
    LoudnessAnalyzer m_analyzer;
    FastFFT m_fft;
    std::vector<std::complex<float>> m_freq;
};

} // namespace Aura::DSP::Analysis
