#pragma once
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace Aura::DSP::Analysis {

/**
 * @class Goniometer
 * @brief Professional Stereo Imaging & Phase Correlation Analyzer.
 * Extracts the relationship between Left and Right channels for stereo balance 
 * and phase compatibility checks (Essential for Logic Pro parity).
 */
class Goniometer {
public:
    static constexpr size_t kHistorySize = 1024;
    struct Data {
        float correlation;
        float balance;
        float xyHistoryL[kHistorySize];
        float xyHistoryR[kHistorySize];
    };

    Goniometer() {
        m_correlation.store(1.0f);
        m_balance.store(0.0f);
        m_historyIdx.store(0);
        for(auto& v : m_historyL) v = 0;
        for(auto& v : m_historyR) v = 0;
    }

    void process(const float* l, const float* r, uint32_t samples) {
        if (samples == 0) return;

        double sumL = 0, sumR = 0, sumLR = 0;
        size_t hIdx = m_historyIdx.load(std::memory_order_relaxed);

        for (uint32_t s = 0; s < samples; ++s) {
            float sL = l[s];
            float sR = r[s];
            
            // 0. UPDATE HISTORY (Circular Buffer for GPU Scope)
            if (s % 4 == 0) { // Subsample for visibility
                m_historyL[hIdx] = sL;
                m_historyR[hIdx] = sR;
                hIdx = (hIdx + 1) % kHistorySize;
            }

            sumL += static_cast<double>(sL * sL);
            sumR += static_cast<double>(sR * sR);
            sumLR += static_cast<double>(sL * sR);
        }
        m_historyIdx.store(hIdx, std::memory_order_relaxed);
        // ... (Existing correlation/balance logic)

        // 1. PHASE CORRELATION (Pearson correlation coefficient approximation)
        double denominator = std::sqrt(sumL * sumR) + 1e-12;
        float corr = static_cast<float>(sumLR / denominator);
        
        // Exponential smoothing (slower for correlation to avoid jitter)
        float prevCorr = m_correlation.load(std::memory_order_relaxed);
        m_correlation.store(0.9f * prevCorr + 0.1f * std::clamp(corr, -1.0f, 1.0f), std::memory_order_relaxed);

        // 2. STEREO BALANCE
        float totalEnergy = static_cast<float>(sumL + sumR) + 1e-12f;
        float bal = (static_cast<float>(sumR) - static_cast<float>(sumL)) / totalEnergy;
        
        float prevBal = m_balance.load(std::memory_order_relaxed);
        m_balance.store(0.8f * prevBal + 0.2f * std::clamp(bal, -1.0f, 1.0f), std::memory_order_relaxed);
    }

    Data getLatest() const {
        Data d;
        d.correlation = m_correlation.load(std::memory_order_relaxed);
        d.balance = m_balance.load(std::memory_order_relaxed);
        for(size_t i=0; i<kHistorySize; ++i) {
            d.xyHistoryL[i] = m_historyL[i]; d.xyHistoryR[i] = m_historyR[i];
        }
        return d;
    }

private:
    std::atomic<float> m_correlation{1.0f};
    std::atomic<float> m_balance{0.0f};
    std::atomic<size_t> m_historyIdx{0};
    float m_historyL[kHistorySize];
    float m_historyR[kHistorySize];
};

} // namespace Aura::DSP::Analysis
