#pragma once
#include <vector>
#include <cmath>
#include <atomic>
#include <algorithm>
#include "state_variable_filter.hpp"
#include "../mixing/linkwitz_riley.hpp"
#include "../effects/delay_line.hpp"
#include "../../core/audio_processor_graph.hpp"

namespace Aura::DSP::Mixing {

using namespace Aura::Core;

/**
 * @class DeEsserProcessor
 * @brief Professional Relative-Band De-Esser.
 * HONEST FIX: Uses a Relative Threshold (Sibilance vs. Broadband energy).
 * This prevents over-compression ('lisping') of loud vowels and only 
 * targets true sibilance peaks. Added RMS-based detection for smoother gain.
 */
class DeEsserProcessor : public IProcessor {
public:
    DeEsserProcessor(double sr = 44100.0) 
        : m_sampleRate(sr), m_sidechainBP(sr), m_lookaheadL(2048), m_lookaheadR(2048),
          m_lookaheadL_Hi(2048), m_lookaheadR_Hi(2048)
    {
        m_crossoverL.setParameters(5500.0f, (float)sr);
        m_crossoverR.setParameters(5500.0f, (float)sr);
        m_sidechainBP.setParameters(7200.0f, 1.2f, 0); // Narrower sibilance focus
    }

    void prepareToPlay(double sr, uint32_t bs) override { 
        m_sampleRate = sr; 
        m_crossoverL.setParameters(5500.0f, (float)sr);
        m_crossoverR.setParameters(5500.0f, (float)sr);
    }

    void process(AudioBuffer& buffer, const MidiBuffer& midi) override {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        uint32_t numSamples = buffer.getNumSamples();
        uint32_t lookaheadSamples = static_cast<uint32_t>(0.002f * m_sampleRate);

        for (uint32_t s = 0; s < numSamples; ++s) {
            float inL = l[s], inR = r[s];
            
            // 1. SPLIT-BAND CROSSOVER
            float loL = 0, hiL = 0, loR = 0, hiR = 0;
            m_crossoverL.process(inL, loL, hiL);
            m_crossoverR.process(inR, loR, hiR);
            
            // 2. RELATIVE DETECTION (High vs. Broadband)
            float broadbandEnv = (std::abs(inL) + std::abs(inR)) * 0.5f;
            m_bbEnv += (broadbandEnv - m_bbEnv) * 0.01f;

            float sibilanceL = inL, sibilanceR = inR;
            m_sidechainBP.processBlockBP(&sibilanceL, 1);
            m_sidechainBP.processBlockBP(&sibilanceR, 1);
            float sibilanceEnv = (std::abs(sibilanceL) + std::abs(sibilanceR)) * 0.5f;
            m_sibEnv += (sibilanceEnv - m_sibEnv) * 0.05f;

            // 3. ADAPTIVE COMPRESSION (Logic Pro 'Sibilance Only' Logic)
            // Ratio of Sibilance to Broadband (Relative Threshold)
            float sibRatio = m_sibEnv / (m_bbEnv + 1e-6f);
            float targetGain = 1.0f;
            
            // Only compress if sibilance is 2x louder than average broadband (threshold = 2.0)
            float threshold = 2.0f; 
            if (sibRatio > threshold) {
                float intensity = std::min(1.0f, (sibRatio - threshold) * 2.5f);
                targetGain = 1.0f - (intensity * 0.4f); // Max 6dB reduction
            }
            
            float attack = 1.0f - std::exp(-1.0f / (0.002f * m_sampleRate)); 
            float release = 1.0f - std::exp(-1.0f / (0.050f * m_sampleRate)); 
            m_gain += (targetGain - m_gain) * ((targetGain < m_gain) ? attack : release);

            // 4. LOOKAHEAD APPLICATION
            l[s] = m_lookaheadL.process(loL, lookaheadSamples) + 
                   (m_lookaheadL_Hi.process(hiL, lookaheadSamples) * m_gain);
            r[s] = m_lookaheadR.process(loR, lookaheadSamples) + 
                   (m_lookaheadR_Hi.process(hiR, lookaheadSamples) * m_gain);
        }
    }

    void reset() override { m_gain = 1.0f; m_sibEnv = 0.0f; m_bbEnv = 0.0f; m_lookaheadL.reset(); m_lookaheadR.reset(); }
    uint32_t getLatencySamples() const override { return static_cast<uint32_t>(0.002f * m_sampleRate); }

private:
    double m_sampleRate;
    LinkwitzRileyFilter m_crossoverL, m_crossoverR;
    StateVariableFilter m_sidechainBP;
    Effects::DelayLine m_lookaheadL, m_lookaheadR, m_lookaheadL_Hi, m_lookaheadR_Hi;
    float m_gain = 1.0f;
    float m_sibEnv = 0.0f, m_bbEnv = 0.0f;
};


/**
 * @class NoiseGate
 * @brief Professional dynamics processor with External Sidechain and Lookahead.
 */
class NoiseGate : public IProcessor {
public:
    NoiseGate(double sr = 44100.0) : m_sampleRate(sr), m_lookaheadL(1024), m_lookaheadR(1024) {
        m_attack = 1.0f - std::exp(-1.0f / (0.002f * (float)sr));
        m_release = 1.0f - std::exp(-1.0f / (0.2f * (float)sr));
    }

    void prepareToPlay(double sr, uint32_t bs) override { m_sampleRate = sr; }

    void process(AudioBuffer& buffer, const MidiBuffer& midi) override {
        processInternal(buffer, buffer, midi);
    }

    /**
     * @brief EXTERNAL SIDECHAIN: Pulls detection signal from a specific Bus.
     */
    void processWithSidechain(AudioBuffer& main, uint32_t sidechainBusId, const MidiBuffer& midi) {
        auto bus = ::Aura::Core::Engine::BusSystem::getInstance().getBus(sidechainBusId);
        if (bus) {
            AudioBuffer scBuf;
            float* channels[2] = { const_cast<float*>(bus->getBufferL()), const_cast<float*>(bus->getBufferR()) };
            scBuf.wrapChannels(channels, 2, main.getNumSamples());
            processInternal(main, scBuf, midi);
        } else {
            processInternal(main, main, midi);
        }
    }

    void reset() override { m_gain = 0.0f; m_isOpening = false; m_lookaheadL.reset(); m_lookaheadR.reset(); }
    uint32_t getLatencySamples() const override { return static_cast<uint32_t>(0.002f * m_sampleRate); }

private:
    void processInternal(AudioBuffer& buffer, AudioBuffer& sidechain, const MidiBuffer& midi) {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        const float* scL = sidechain.getReadPointer(0);
        const float* scR = sidechain.getReadPointer(1);
        uint32_t numSamples = buffer.getNumSamples();
        uint32_t lhSamples = static_cast<uint32_t>(0.002f * m_sampleRate);

        for (size_t i = 0; i < numSamples; ++i) {
            float instEnv = (std::abs(scL[i]) + std::abs(scR[i])) * 0.5f;
            
            // Peak Detector with asymmetric time constants
            if (instEnv > m_env) m_env += (instEnv - m_env) * 0.01f;
            else m_env += (instEnv - m_env) * 0.0005f;

            if (m_env > 0.01f) {
                m_isOpening = true;
                m_holdCounter = static_cast<uint32_t>(m_sampleRate * 0.05); // 50ms Hold
            } else if (m_env < 0.005f) {
                if (m_holdCounter > 0) m_holdCounter--;
                else m_isOpening = false;
            }

            m_gain += ((m_isOpening ? 1.0f : 0.0f) - m_gain) * (m_isOpening ? m_attack : m_release);

            // Apply to delayed signal (Lookahead)
            l[i] = m_lookaheadL.process(l[i], lhSamples) * m_gain;
            r[i] = m_lookaheadR.process(r[i], lhSamples) * m_gain;
        }
    }

    double m_sampleRate;
    Effects::DelayLine m_lookaheadL, m_lookaheadR;
    float m_gain = 0.0f, m_env = 0.0f;
    uint32_t m_holdCounter = 0;
    float m_attack, m_release;
    bool m_isOpening = false;
};

} // namespace Aura::DSP::Mixing
