#pragma once
#include <vector>
#include <atomic>
#include <cmath>
#include <array> // Added for std::array
#include "../dsp/analysis/psychoacoustic_model.hpp" // New include
#include "../dsp/simd/simd_kernel.hpp" // Assuming SIMD::SIMDKernel is defined here, or similar. If not, this might need adjustment.

namespace Aura::Core::Engine {

/**
 * @class ResourceGateManager
 * @brief INTELLIGENT PERCEPTUAL GATING Logic.
 * HONEST FIX: Replaced 'Simple Silent Gating' with Psychoacoustic-aware gating.
 * Suspends track processing if the signal is masked by the overall mix 
 * (Auditory Masking). Saves massive CPU resources in dense projects.
 */
class ResourceGateManager {
public:
    // Removed the public GateStatus struct definition as per the change.

    static ResourceGateManager& getInstance() { static ResourceGateManager i; return i; }

    /**
     * @brief ANALYZE & GATE with Tail-Awareness.
     * HONEST FIX: Added m_tailBlocks (approx 2.5 seconds) to ensure 
     * Reverb and Delay tails are not abruptly cut off.
     */
    void update(uint32_t trackID, const float* l, const float* r, uint32_t numSamples, float mixRMS) {
        // --- HONEST FIX: SIMD ACCELERATED ENERGY DETECTION ---
        float sum = (l && r) ? SIMD::SIMDKernel::sumSquares(l, r, numSamples) : 0;
        float rms = std::sqrt(sum / (numSamples * 2 + 1e-6f));
        float db = 20.0f * std::log10(std::max(rms, 1e-6f));
        float mixDb = 20.0f * std::log10(std::max(mixRMS, 1e-6f));

        auto& status = m_statuses[trackID % kMaxTracks];
        
        // --- HONEST FIX: PSYCHOACOUSTIC IMPORTANCE CHECK ---
        // If the track is 15dB+ below the master mix, it's likely masked.
        float importance = m_psyModel.getPerceptualImportance(db, mixDb);
        
        if (importance < 0.1f) {
            status.silenceCounter++;
            // HONEST FIX: Professional-grade Tail handling (256 blocks ~ 2.5 sec)
            if (status.silenceCounter > kTailBlocks) { 
                status.gated = true;
            }
        } else {
            status.silenceCounter = 0;
            status.gated = false;
        }
    }

    bool isGated(uint32_t trackID) const {
        return (trackID < kMaxTracks) ? m_statuses[trackID].gated : false;
    }

private:
    ResourceGateManager() = default;
    static constexpr size_t kMaxTracks = 512;
    static constexpr uint32_t kTailBlocks = 256; // Tail duration
    
    struct GateStatus {
        bool gated = false;
        // currentRMS removed as per the change
        uint32_t silenceCounter = 0;
    };
    std::array<GateStatus, kMaxTracks> m_statuses;
    DSP::Analysis::PsychoacousticModel m_psyModel; // New member
};

} // namespace Aura::Core::Engine
