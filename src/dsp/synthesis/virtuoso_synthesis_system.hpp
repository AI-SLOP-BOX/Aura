#pragma once

#include <memory>
#include <vector>
#include "virtuoso_orchestra.hpp"
#include "aura_sampler_pro.hpp"
#include "synthesis_core.hpp"

namespace Aura::Core::DSP::Synthesis {

/**
 * @brief VirtuosoSynthesisSystem: The master synthesis hub of the DAW.
 * Orchestrates physical modeling, sampling, and subtractive synthesis into a single high-performance engine.
 */
class VirtuosoSynthesisSystem {
public:
    explicit VirtuosoSynthesisSystem(double sr) : m_sampleRate(sr) {
        m_orchestra = std::make_unique<VirtuosoOrchestra>(sr);
        m_sampler = std::make_unique<AuraSamplerPro>(sr);
        m_synth = std::make_unique<SubtractiveSynth>(sr);
    }

    /**
     * @brief High-level trigger for complex instrument patches.
     */
    void triggerInstrument(const std::string& patchName, float pitch, float velocity) {
        // PROFESSIONAL RULE: A single "Patch" can trigger multiple engines (Layering).
        if (patchName == "Hybrid Piano") {
            m_orchestra->noteOn(VirtuosoOrchestra::Model::Piano, pitch, velocity);
            m_sampler->trigger(1.0f, velocity * 0.5f); // Layering a recorded sample for richness
        } else {
            m_synth->noteOn(pitch, velocity);
        }
    }

    /**
     * @brief Mixes and renders all synthesis layers (Real-time thread).
     */
    void render(float* l, float* r, size_t numFrames) {
        m_orchestra->render(l, r, numFrames);
        m_sampler->process(l, numFrames); // Simplified: Assumes mono sampler for now
        m_synth->render(l, r, numFrames);
    }

private:
    double m_sampleRate;
    std::unique_ptr<VirtuosoOrchestra> m_orchestra;
    std::unique_ptr<AuraSamplerPro> m_sampler;
    std::unique_ptr<SubtractiveSynth> m_synth;
};

} // namespace Aura::Core::DSP::Synthesis
