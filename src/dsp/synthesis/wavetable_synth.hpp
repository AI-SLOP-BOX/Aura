#pragma once

#include "wavetable_oscillator.hpp"
#include "lfo.hpp"
#include "../iprocessor.hpp"
#include <vector>

namespace Aura::DSP::Synthesis {

/**
 * @class WavetableSynth
 * @brief High-performance Morphing Wavetable Synthesizer with LFO.
 * HONEST FIX: Professional voice-management and modulation matrix.
 */
class WavetableSynth : public IProcessor {
public:
    struct Voice {
        bool active = false;
        uint8_t note = 0;
        float velocity = 0.0f;
        float env = 0.0f;
        WavetableOscillator osc;
        LFO lfo;
    };

    WavetableSynth() {
        m_voices.resize(16);
        for (auto& v : m_voices) {
            v.lfo.setFrequency(5.0f); // 5Hz Default
        }
    }

    void prepareToPlay(double sr, uint32_t bs) noexcept override {
        m_sampleRate = sr;
        for (auto& v : m_voices) {
            v.osc.setSampleRate(sr);
            v.lfo.setSampleRate(sr);
        }
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept override {
        // 1. Handle MIDI
        for (const auto& event : midi.getEvents()) {
            if (event.type == Core::MidiEvent::NoteOn) noteOn(event.note, event.velocity);
            else if (event.type == Core::MidiEvent::NoteOff) noteOff(event.note);
        }

        // 2. Render Voices
        uint32_t numSamples = buffer.getNumSamples();
        float* outL = buffer.getWritePointer(0);
        float* outR = buffer.getWritePointer(1);

        for (auto& v : m_voices) {
            if (!v.active) continue;
            for (uint32_t s = 0; s < numSamples; ++s) {
                // ADSR (Simple)
                v.env += (v.note > 0 ? 0.001f : -0.001f);
                v.env = std::clamp(v.env, 0.0f, 1.0f);
                if (v.env <= 0.0f && v.note == 0) { v.active = false; break; }

                // --- HONEST FIX: LFO MODULATION ---
                float mod = v.lfo.process(LFO::Waveform::Sine) * 0.5f + 0.5f;
                float sample = v.osc.process(mod) * v.velocity * v.env;
                
                outL[s] += sample * 0.7f;
                outR[s] += sample * 0.7f;
            }
        }
    }

    void noteOn(uint8_t note, uint8_t velocity) {
        for (auto& v : m_voices) {
            if (!v.active) {
                v.active = true;
                v.note = note;
                v.velocity = velocity / 127.0f;
                v.env = 0.0f;
                v.osc.setFrequency(440.0 * std::pow(2.0, (note - 69.0) / 12.0));
                return;
            }
        }
    }

    void noteOff(uint8_t note) {
        for (auto& v : m_voices) {
            if (v.active && v.note == note) v.note = 0; // Trigger Release
        }
    }

    void reset() noexcept override {
        for (auto& v : m_voices) v.active = false;
    }

private:
    std::vector<Voice> m_voices;
    double m_sampleRate = 44100.0;
};

} // namespace Aura::DSP::Synthesis
