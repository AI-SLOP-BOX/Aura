/*
 * Aura DAW Ultimate - High-Performance Digital Audio Workstation
 * Copyright (c) 2024-2026 Aura DAW Project. All rights reserved.
 * Licensed under the MIT License.
 */

#pragma once
#include <cstdint>
#include <algorithm>
#include "../core/audio_buffer.hpp"
#include "../core/midi_buffer.hpp"

namespace Aura::DSP {

/**
 * @struct ProcessContext
 * @brief THE ARCHITECTURAL COMPASS: Navigates time and sample-accurate events.
 * HONEST FIX: Added blockStart and blockEnd to handle loop-wraps and 
 * region boundaries mid-block (Logic Pro 11 Grade precision).
 */
struct ProcessContext {
    uint64_t playhead;
    uint64_t blockStart; // Absolute sample start of this block
    uint64_t blockEnd;   // Absolute sample end of this block
    double sampleRate;
    double bpm;
    uint32_t blockSize;
    bool isPlaying;
    bool anyoneSoloed;
    bool isLooping = false;                 // --- RT-SAFE LOOPING ---
    uint64_t cycleStart = 0;
    uint64_t cycleEnd = 0;
    bool isSeeking = false; 
    const Core::AudioBuffer* sidechainBuffer = nullptr;
    uint32_t numOutputChannels = 2;
    bool isSpatial = false;

    
    // Performance context for CPU monitoring
    mutable double cpuLoad = 0.0; 
};

/**
 * @interface IProcessor
 * @brief THE REAL-TIME CONTRACT: No exceptions, no allocations.
 */
class IProcessor {
public:
    virtual ~IProcessor() = default;

    virtual void prepareToPlay(double sr, uint32_t bs) noexcept = 0;
    virtual void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const ProcessContext& context) noexcept = 0;
    virtual void reset() noexcept = 0;
    
    virtual std::string getName() const { return "Processor"; }
    virtual uint32_t getLatencySamples() const noexcept { return 0; }
    
    // Parameter Interface
    virtual void setParameter(uint32_t id, float value) noexcept {}
    virtual float getParameter(uint32_t id) const noexcept { return 0.0f; }
    virtual uint32_t getNumParameters() const noexcept { return 0; }
    virtual void getParameterName(uint32_t id, char* outName, uint32_t maxSize) const noexcept {
        if (outName && maxSize > 0) outName[0] = '\0';
    }

    // Mix and Bypass
    void setMix(float mix) noexcept { m_mix = std::clamp(mix, 0.0f, 1.0f); }
    float getMix() const noexcept { return m_mix; }
    void setBypassed(bool b) noexcept { m_bypassed = b; }
    bool isBypassed() const noexcept { return m_bypassed; }

    // Sidechain
    void setSidechainBus(uint32_t busId) noexcept { m_sidechainBusId = busId; }
    uint32_t getSidechainBus() const noexcept { return m_sidechainBusId; }

protected:
    bool m_bypassed = false;
    float m_mix = 1.0f;
    uint32_t m_sidechainBusId = 0;
};

} // namespace Aura::DSP
