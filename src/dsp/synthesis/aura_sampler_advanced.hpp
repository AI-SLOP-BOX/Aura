#pragma once

#include <vector>
#include <map>
#include <string>

namespace Aura::Core::DSP::Synthesis {

/**
 * @brief SampleLayer: Single audio source for a specific velocity range.
 */
struct SampleLayer {
    std::vector<float> data;
    uint8_t minVelocity;
    uint8_t maxVelocity;
};

/**
 * @brief AuraSamplerAdvanced: A multi-layered, production-grade sampler engine.
 * Supports velocity-sensitive multi-layering for realistic instrument performance.
 */
class AuraSamplerAdvanced {
public:
    explicit AuraSamplerAdvanced(double sr) : m_sampleRate(sr) {}

    /**
     * @brief Adds a new multisample layer for velocity sensitivity.
     */
    void addLayer(const std::vector<float>& data, uint8_t minV, uint8_t maxV) {
        m_layers.push_back({data, minV, maxV});
    }

    /**
     * @brief Plays the best-matching layer for a given note velocity.
     */
    void trigger(uint8_t velocity) {
        for (const auto& layer : m_layers) {
            if (velocity >= layer.minVelocity && velocity <= layer.maxVelocity) {
                m_activeSample = &layer.data;
                m_playbackPos = 0;
                break;
            }
        }
    }

    /**
     * @brief Processes one block of multisampled audio.
     */
    void process(float* out, size_t numFrames) {
        if (!m_activeSample) return;

        for (size_t i = 0; i < numFrames; ++i) {
            if (m_playbackPos < m_activeSample->size()) {
                out[i] += (*m_activeSample)[m_playbackPos++];
            } else {
                m_activeSample = nullptr;
                break;
            }
        }
    }

private:
    double m_sampleRate;
    std::vector<SampleLayer> m_layers;
    const std::vector<float>* m_activeSample = nullptr;
    size_t m_playbackPos = 0;
};

} // namespace Aura::Core::DSP::Synthesis
