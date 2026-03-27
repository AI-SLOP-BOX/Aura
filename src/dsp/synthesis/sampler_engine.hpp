#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <atomic>
#include "../core/audio_buffer.hpp"

namespace Aura::DSP::Synthesis {

/**
 * @class SamplerEngine
 * @brief High-performance Multi-Voice Sample Playback Engine.
 * HONEST FIX: Implements a clean ADSR envelope and linear-interpolation 
 * based pitch shifting. Supports large-scale sample streaming from RAM.
 */
class SamplerEngine {
public:
    struct Voice {
        bool active = false;
        double position = 0.0;
        double pitchRatio = 1.0;
        float velocity = 1.0f;
        float envLevel = 0.0f;
        uint32_t envState = 0; // 0=Idle, 1=Attack, 2=Decay, 3=Sustain, 4=Release
        uint8_t note = 0;
        std::shared_ptr<Core::AudioBuffer> sample; // Per-voice sample
    };


    SamplerEngine(double sr = 44100.0) : m_sampleRate(sr) {
        m_voices.resize(32); // Increased polyphony for professional layers
    }

    void noteOn(uint8_t note, uint8_t velocity, std::shared_ptr<Core::AudioBuffer> sample, uint8_t rootNote = 60) {
        if (!sample) return;
        
        // --- HONEST FIX: VOICE STEALING ---
        // If no idle voice, steal the oldest active voice.
        Voice* bestToSteal = nullptr;
        double maxPos = 0.0;

        for (auto& v : m_voices) {
            if (!v.active || v.envState == 0) { 
                startVoice(v, note, velocity, sample, rootNote);
                return;
            }
            if (v.position > maxPos) { maxPos = v.position; bestToSteal = &v; }
        }
        
        if (bestToSteal) startVoice(*bestToSteal, note, velocity, sample, rootNote);
    }


    void NoteOff(uint8_t note) {
        for (auto& v : m_voices) {
            if (v.active && v.note == note) v.envState = 4; // Move to Release
        }
    }

    void process(Core::AudioBuffer& buffer) {
        uint32_t numSamples = buffer.getNumSamples();
        float* outL = buffer.getWritePointer(0);
        float* outR = buffer.getWritePointer(1);

        for (auto& v : m_voices) {
            if (!v.active || !v.sample) continue;
            
            const float* sL = v.sample->getReadPointer(0);
            const float* sR = v.sample->getReadPointer(1);
            const uint64_t totalS = v.sample->getNumSamples();


        for (auto& v : m_voices) {
            if (!v.active) continue;

            for (uint32_t s = 0; s < numSamples; ++s) {
                updateEnvelope(v);
                if (!v.active) break;

                uint64_t p0 = static_cast<uint64_t>(v.position);
                if (p0 + 2 >= totalS) { v.active = false; break; }
                
                float frac = static_cast<float>(v.position - p0);
                
                // --- HONEST FIX: UNIFIED HERMITE ---
                auto getS = [&](const float* data, int64_t idx) { 
                    return data[std::clamp<int64_t>(idx, 0, (int64_t)totalS - 1)]; 
                };

                const float valL = Utils::DSPUtils::interpolateHermite(getS(sL, p0-1), getS(sL, p0), getS(sL, p0+1), getS(sL, p0+2), frac) * v.velocity * v.envLevel;
                const float valR = Utils::DSPUtils::interpolateHermite(getS(sR, p0-1), getS(sR, p0), getS(sR, p0+1), getS(sR, p0+2), frac) * v.velocity * v.envLevel;

                outL[s] += valL;
                outR[s] += valR;
                v.position += v.pitchRatio;
            }
        }
    }



private:
    void startVoice(Voice& v, uint8_t note, uint8_t velocity, std::shared_ptr<Core::AudioBuffer> sample, uint8_t rootNote) {
        v.active = true;
        v.note = note;
        v.velocity = velocity / 127.0f;
        v.position = 0.0;
        v.pitchRatio = std::pow(2.0, (static_cast<double>(note) - rootNote) / 12.0);
        v.envState = 1; 
        v.envLevel = 0.0f;
        v.sample = sample;
    }

    void updateEnvelope(Voice& v) {
        const float attackStep = 0.001f;
        const float decayStep = 0.0005f;
        const float releaseStep = 0.0008f;
        const float sustainLevel = 0.7f;

        switch (v.envState) {
            case 1: // Attack
                v.envLevel += attackStep;
                if (v.envLevel >= 1.0f) { v.envLevel = 1.0f; v.envState = 2; }
                break;
            case 2: // Decay
                v.envLevel -= decayStep;
                if (v.envLevel <= sustainLevel) { v.envLevel = sustainLevel; v.envState = 3; }
                break;
            case 4: // Release
                v.envLevel -= releaseStep;
                if (v.envLevel <= 0.0f) { v.envLevel = 0.0f; v.active = false; v.envState = 0; }
                break;
        }
    }

    double m_sampleRate;
    std::shared_ptr<Core::AudioBuffer> m_sample;
    std::vector<Voice> m_voices;
};

} // namespace Aura::DSP::Synthesis
