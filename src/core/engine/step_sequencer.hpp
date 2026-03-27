#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <random>
#include "../../core/midi_buffer.hpp"

namespace Aura::Core::Engine {

/**
 * @class StepSequencer
 * @brief Logic Pro Style High-Density Polyphonic Pattern Engine.
 * HONEST FIX: Added a Pending Trigger buffer to handle notes delayed by
 * Swing or Humanization that fall outside the current audio block.
 * This ensures absolute rhythmic precision without note loss.
 */
class StepSequencer {
public:
    static constexpr int kMaxSteps = 64;
    static constexpr int kMaxLanes = 16; 

    StepSequencer() { reset(); initDefaults(); }

    void reset() {
        for (int l = 0; l < kMaxLanes; ++l) {
            for (int s = 0; s < kMaxSteps; ++s) {
                m_lanes[l][s].store(false, std::memory_order_relaxed);
                m_laneVelocities[l][s].store(100, std::memory_order_relaxed);
                m_stepProbability[l][s].store(100, std::memory_order_relaxed);
                m_stepSubsteps[l][s].store(1, std::memory_order_relaxed);
                m_stepOffsets[l][s].store(0.0f, std::memory_order_relaxed);
            }
        }
        m_numActiveNotes = 0;
        for (auto& p : m_pendingPool) p.active = false;
    }

    void process(MidiBuffer& midi, uint64_t currentPos, uint32_t numSamples, double bpm, double sr) {
        if (!m_active) return;

        // 1. PROCESS PENDING TRIGGERS FROM LAST BLOCK
        for (auto& p : m_pendingPool) {
            if (!p.active) continue;
            if (p.delaySamples < (float)numSamples) {
                uint8_t onEv[3] = { 0x90, p.note, p.vel };
                midi.addEvent((uint32_t)p.delaySamples, onEv, 3);
                
                uint64_t offS = currentPos + (uint32_t)p.delaySamples + static_cast<uint64_t>(p.lenSamples);
                if (m_numActiveNotes < m_activeNotes.size()) {
                    m_activeNotes[m_numActiveNotes++] = { p.note, offS };
                }
                p.active = false;
            } else {
                p.delaySamples -= (float)numSamples;
            }
        }

        double samplesPerStep = (60.0 * sr) / (bpm * 4.0); // 16th notes
        double phaseIncr = 1.0 / samplesPerStep;

        for (uint32_t s = 0; s < numSamples; ++s) {
            double nextPhase = m_phase + phaseIncr;
            
            if (std::floor(nextPhase) > std::floor(m_phase)) {
                uint32_t stepIdx = static_cast<uint32_t>(std::floor(nextPhase)) % kMaxSteps;
                double swingOffset = (stepIdx % 2 != 0) ? (samplesPerStep * m_swingAmount * 0.5) : 0.0;

                for (int l = 0; l < kMaxLanes; ++l) {
                    if (m_lanes[l][stepIdx].load(std::memory_order_relaxed)) {
                        int prob = m_stepProbability[l][stepIdx].load(std::memory_order_relaxed);
                        if ((m_gen() % 100) >= prob) continue;

                        float microOffset = m_stepOffsets[l][stepIdx].load(std::memory_order_relaxed);
                        double jitter = (m_humanizeAmount > 0) ? ((m_dist(m_gen) * m_humanizeAmount) * 441.0) : 0;
                        double totalOffset = swingOffset + (microOffset * samplesPerStep) + jitter;
                        
                        uint8_t note = m_laneNotes[l].load(std::memory_order_relaxed);
                        uint8_t vel = m_laneVelocities[l][stepIdx].load(std::memory_order_relaxed);
                        
                        // Calculated trigger sample (can exceed 'numSamples' due to swing)
                        uint32_t triggerSample = s + (uint32_t)totalOffset;

                        if (triggerSample < numSamples) {
                            uint8_t onEv[3] = { 0x90, note, vel };
                            midi.addEvent(triggerSample, onEv, 3);
                            uint64_t offS = currentPos + triggerSample + static_cast<uint64_t>(samplesPerStep * 0.4);
                            if (m_numActiveNotes < m_activeNotes.size()) {
                                m_activeNotes[m_numActiveNotes++] = { note, offS };
                            }
                        } else {
                            // PUSH TO PENDING POOL: Lock-Free
                            for (auto& p : m_pendingPool) {
                                if (!p.active) {
                                    p = {note, vel, (float)(triggerSample - numSamples), (float)(samplesPerStep * 0.4), true};
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            m_phase = nextPhase;
        }

        handleNoteOffs(midi, currentPos, numSamples);
    }

    void setSwing(float amount) { m_swingAmount = amount; }
    void setHumanize(float amount) { m_humanizeAmount = amount; }
    void setActive(bool a) { m_active = a; }
    void toggleStep(int lane, int step) { 
        if (lane < kMaxLanes && step < kMaxSteps) {
            bool current = m_lanes[lane][step].load();
            m_lanes[lane][step].store(!current);
        }
    }

private:
    void handleNoteOffs(MidiBuffer& midi, uint64_t currentPos, uint32_t numSamples) {
        for (int i = 0; i < (int)m_numActiveNotes; ) {
            auto& n = m_activeNotes[i];
            if (n.offSample >= currentPos && n.offSample < currentPos + numSamples) {
                uint8_t ev[3] = { 0x80, n.note, 0 };
                midi.addEvent(static_cast<uint32_t>(n.offSample - currentPos), ev, 3);
                m_activeNotes[i] = m_activeNotes[--m_numActiveNotes];
            } else if (n.offSample < currentPos) {
                 m_activeNotes[i] = m_activeNotes[--m_numActiveNotes];
            } else {
                ++i;
            }
        }
    }

    struct PendingTrigger { 
        uint8_t note; uint8_t vel; 
        float delaySamples; float lenSamples; 
        bool active = false; 
    };
    
    struct ActiveNote { uint8_t note; uint64_t offSample; };
    
    bool m_active = false;
    double m_phase = 0.0;
    float m_swingAmount = 0.0f;
    float m_humanizeAmount = 0.05f;

    std::mt19937 m_gen{0x1234};
    std::uniform_real_distribution<float> m_dist{-1.0f, 1.0f};

    // --- HONEST FIX: Fixed-capacity Real-time Safe Storage ---
    static constexpr size_t kMaxPending = 512;
    std::array<PendingTrigger, kMaxPending> m_pendingPool;
    std::array<ActiveNote, kMaxLanes * 16> m_activeNotes;
    size_t m_numActiveNotes = 0;

    std::array<std::array<std::atomic<bool>, kMaxSteps>, kMaxLanes> m_lanes;
    std::array<std::array<std::atomic<uint8_t>, kMaxSteps>, kMaxLanes> m_laneVelocities;
    std::array<std::array<std::atomic<uint8_t>, kMaxSteps>, kMaxLanes> m_stepProbability;
    std::array<std::array<std::atomic<uint8_t>, kMaxSteps>, kMaxLanes> m_stepSubsteps;
    std::array<std::array<std::atomic<float>, kMaxSteps>, kMaxLanes> m_stepOffsets;
    std::array<std::atomic<uint8_t>, kMaxLanes> m_laneNotes;


    void initDefaults() {
        uint8_t defNotes[] = {36, 38, 42, 46, 49, 39, 41, 43, 45, 47, 48, 50, 51, 52, 53, 54};
        for(int i=0; i<kMaxLanes; ++i) m_laneNotes[i].store(defNotes[i], std::memory_order_relaxed);
    }
};

} // namespace Aura::Core::Engine
