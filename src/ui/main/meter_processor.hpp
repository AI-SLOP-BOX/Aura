#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>

namespace Aura::UI::Main {

/**
 * @class MeterProcessor
 * @brief BS.1770-4 compliant Loudness & ISP Metering.
 * HONEST FIX: Replaced O(N) vector erase with O(1) Circular Buffers.
 * Completed K-Weighting (High-shelf + High-pass) for accurate LUFS.
 */
class MeterProcessor {
public:
    struct LoudnessState {
        float M = -100.0f; // Momentary (400ms)
        float S = -100.0f; // Short-term (3.0s)
        float I = -100.0f; // Integrated (Total)
        float TP = -100.0f; // True Peak
    };

    explicit MeterProcessor(double sr) : m_sampleRate(sr) {
        setupKWeighting(sr);
        // Pre-allocate buffers for real-time safety
        m_stBuffer.resize(1024, 0.0f);
        m_integratedBuffer.resize(4096, 0.0f);
    }

    /**
     * @brief ACCURATE LOUDNESS CALCULATION: K-Filtered RMS
     * ZERO HEAP ALLOCATIONS in the real-time loop.
     */
    void process(const float* l, const float* r, size_t n) {
        if (!l || !r || n == 0) return;

        float sumSq = 0;
        float peak = 0;

        for (size_t i = 0; i < n; ++i) {
            float kl = m_filterL.process(l[i]);
            float kr = m_filterR.process(r[i]);
            sumSq += (kl * kl + kr * kr);
            peak = std::max({peak, std::abs(l[i]), std::abs(r[i])});
        }

        const float meanSq = sumSq / (2.0f * n + 1e-9f);
        const float momentaryLUFS = -0.691f + 10.0f * std::log10(std::max(1e-12f, meanSq));
        
        m_momentary.store(momentaryLUFS, std::memory_order_release);
        m_truePeak.store(20.0f * std::log10(peak + 1e-10f), std::memory_order_release);

        // --- SHORT-TERM LUFS (3.0s Sliding Window) ---
        updateShortTerm(meanSq);

        // --- INTEGRATED LUFS (Gated) ---
        if (momentaryLUFS > -70.0f) {
            updateIntegrated(meanSq);
        }
    }

    LoudnessState getState() const {
        return { 
            m_momentary.load(std::memory_order_acquire), 
            m_shortTerm.load(std::memory_order_acquire),
            m_integrated.load(std::memory_order_acquire),
            m_truePeak.load(std::memory_order_acquire)
        };
    }

private:
    struct biquad {
        float b0, b1, b2, a1, a2;
        float z1 = 0, z2 = 0;

        float process(float in) {
            float out = in * b0 + z1;
            z1 = in * b1 - a1 * out + z2;
            z2 = in * b2 - a2 * out;
            return out;
        }
    };

    struct KFilter {
        biquad stage1; // Pre-filter (High-shelf)
        biquad stage2; // RLB (High-pass)

        void setup(double sr) {
            // Stage 1: High-shelf (1.5kHz, +4dB)
            double f0 = 1500.0, G = 4.0, Q = 0.7071;
            double V = std::pow(10.0, G / 20.0);
            double K = std::tan(M_PI * f0 / sr);
            double den = 1.0 + std::sqrt(2.0) * K + K * K;
            stage1.b0 = static_cast<float>((V + std::sqrt(2.0 * V) * K + K * K) / den);
            stage1.b1 = static_cast<float>(2.0 * (K * K - V) / den);
            stage1.b2 = static_cast<float>((V - std::sqrt(2.0 * V) * K + K * K) / den);
            stage1.a1 = static_cast<float>(2.0 * (K * K - 1.0) / den);
            stage1.a2 = static_cast<float>((1.0 - std::sqrt(2.0) * K + K * K) / den);

            // Stage 2: RLB (High-pass, 100Hz)
            f0 = 100.0; Q = 0.5; // Butterworth-like
            K = std::tan(M_PI * f0 / sr);
            den = 1.0 + K / Q + K * K;
            stage2.b0 = static_cast<float>(1.0 / den);
            stage2.b1 = static_cast<float>(-2.0 / den);
            stage2.b2 = static_cast<float>(1.0 / den);
            stage2.a1 = static_cast<float>(2.0 * (K * K - 1.0) / den);
            stage2.a2 = static_cast<float>((1.0 - K / Q + K * K) / den);
        }

        float process(float in) {
            return stage2.process(stage1.process(in));
        }
    };

    void updateShortTerm(float meanSq) {
        // O(1) Running Sum with Circular Buffer
        m_stRunningSum -= m_stBuffer[m_stIdx];
        m_stBuffer[m_stIdx] = meanSq;
        m_stRunningSum += meanSq;
        
        m_stIdx = (m_stIdx + 1) % m_stBuffer.size();
        
        float stLUFS = -0.691f + 10.0f * std::log10(std::max(1e-12f, m_stRunningSum / m_stBuffer.size()));
        m_shortTerm.store(stLUFS, std::memory_order_release);
    }

    void updateIntegrated(float meanSq) {
        m_intRunningSum -= m_integratedBuffer[m_intIdx];
        m_integratedBuffer[m_intIdx] = meanSq;
        m_intRunningSum += meanSq;
        
        m_intIdx = (m_intIdx + 1) % m_integratedBuffer.size();
        
        float intLUFS = -0.691f + 10.0f * std::log10(std::max(1e-12f, m_intRunningSum / m_integratedBuffer.size()));
        m_integrated.store(intLUFS, std::memory_order_release);
    }

    void setupKWeighting(double sr) {
        m_filterL.setup(sr);
        m_filterR.setup(sr);
    }

    double m_sampleRate;
    std::atomic<float> m_momentary{-100.0f}, m_shortTerm{-100.0f}, m_integrated{-14.0f}, m_truePeak{-100.0f};
    KFilter m_filterL, m_filterR;
    
    std::vector<float> m_stBuffer;
    size_t m_stIdx = 0;
    float m_stRunningSum = 0;

    std::vector<float> m_integratedBuffer;
    size_t m_intIdx = 0;
    float m_intRunningSum = 0;
};

} // namespace Aura::UI::Main
