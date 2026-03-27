#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <cmath>
#include "track.hpp"
#include "../../dsp/analysis/loudness_meter.hpp"
#include "../../dsp/utils/dsp_utils.hpp"

namespace Aura::Core::Engine {

/**
 * @class MixAssistant
 * @brief High-end Calculation-based AI Mixing Assistant (Gain Staging).
 * HONEST FIX: Optimized the analysis pass to reuse buffers and fixed the O(N^2) applying loop.
 */
class MixAssistant {
public:
    struct TrackRecommendation {
        uint32_t trackId;
        std::string name;
        float currentLUFS;
        float suggestedGainDB;
    };

    /**
     * @brief ANALYZE (Professional Offline Pass).
     */
    static std::vector<TrackRecommendation> analyzeProject(const std::vector<std::shared_ptr<Track>>& tracks, double sampleRate, float targetLUFS = -18.0f) {
        std::vector<TrackRecommendation> recommendations;
        recommendations.reserve(tracks.size());
        
        const uint32_t blockSize = 512;
        Core::AudioBuffer scanBuf(2, blockSize);
        DSP::Analysis::LoudnessMeter meter;
        meter.prepare(sampleRate);

        for (auto& t : tracks) {
            if (t->isBus()) continue;

            meter.reset();
            
            // ANALYZE 30s of processed audio
            const uint32_t totalSamples = static_cast<uint32_t>(sampleRate * 30.0);
            
            for (uint32_t s = 0; s < totalSamples; s += blockSize) {
                scanBuf.clear();
                // HONEST FIX: We must call process() to include the plugins!
                // fetchAudio only gets the raw regions.
                t->fetchAudio(scanBuf.getWritePointer(0), scanBuf.getWritePointer(1), s, blockSize);
                t->process(scanBuf, blockSize, s, sampleRate);
                
                meter.process(scanBuf, blockSize);
            }
            
            float currentLUFS = meter.getIntegratedLUFS();
            if (currentLUFS < -100.0f) currentLUFS = -100.0f; 

            float diff = targetLUFS - currentLUFS;
            recommendations.push_back({ t->getId(), t->getName(), currentLUFS, diff });
        }
        return recommendations;
    }

    /**
     * @brief APPLY: Optimized O(N) application.
     */
    static void applyLevels(std::vector<std::shared_ptr<Track>>& tracks, const std::vector<TrackRecommendation>& recs) {
        // Create a fast lookup map
        std::unordered_map<uint32_t, float> gainMap;
        for (const auto& r : recs) {
            gainMap[r.trackId] = DSP::Utils::DSPUtils::dbToLinear(r.suggestedGainDB);
        }

        for (auto& t : tracks) {
            auto it = gainMap.find(t->getId());
            if (it != gainMap.end()) {
                t->setVolume(t->getVolume() * it->second);
            }
        }
    }
};

} // namespace Aura::Core::Engine
