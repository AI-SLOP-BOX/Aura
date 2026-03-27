#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "../iprocessor.hpp"
#include "../../core/engine/bus_system.hpp"

namespace Aura::DSP::Effects {

/**
 * @class SidechainDuck
 * @brief Dynamic Pumping effect for professional Electronic/Trap music.
 * HONEST FIX: Uses external sidechain bus or internal LFO (Sync'd to BPM).
 * Provides 'The Bounce' found in modern Logic Pro productions.
 */
class SidechainDuck : public IProcessor {
public:
    SidechainDuck() : m_lfoIdx(0.0) {}

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        if (m_bypassed) return;

        uint32_t numSamples = buffer.getNumSamples();
        double samplesPerBeat = (60.0 / context.bpm) * context.sampleRate;
        
        // 1. SOURCE DETECTION
        bool useExternal = (m_sidechainBusId != 0);
        const float* sideL = nullptr;
        const float* sideR = nullptr;
        
        if (useExternal) {
            auto bus = Core::Engine::BusSystem::getInstance().getBus(m_sidechainBusId);
            if (bus) {
                sideL = bus->getBufferL();
                sideR = bus->getBufferR();
            }
        }

        // 2. APPLY DUCKING
        for (uint32_t s = 0; s < numSamples; ++s) {
            float reduction = 0.0f;

            if (useExternal && sideL) {
                // EXTERNAL MODE: Peak detection on Sidechain
                float peak = std::max(std::abs(sideL[s]), std::abs(sideR[s]));
                reduction = std::clamp(peak / 0.5f, 0.0f, 1.0f);
            } else {
                // INTERNAL mode: BPM sync LFO (Quarter note pump)
                double phase = std::fmod((context.playhead + s) / samplesPerBeat, 1.0);
                reduction = static_cast<float>(std::pow(1.0 - phase, 2.0)); 
            }

            float gain = 1.0f - (reduction * m_depth);
            m_currentGain = 0.99f * m_currentGain + 0.01f * gain;

            buffer.getWritePointer(0)[s] *= m_currentGain;
            buffer.getWritePointer(1)[s] *= m_currentGain;
        }
    }

    void reset() noexcept override {
        m_currentGain = 1.0f;
        m_lfoIdx = 0.0;
    }

    void setDepth(float d) { m_depth = std::clamp(d, 0.0f, 1.0f); }

private:
    double m_sampleRate = 44100.0;
    float m_depth = 0.8f;
    float m_currentGain = 1.0f;
    double m_lfoIdx;
};

} // namespace Aura::DSP::Effects
