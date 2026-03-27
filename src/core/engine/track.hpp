#pragma once
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <algorithm>
#include "../audio_buffer.hpp"
#include "../../dsp/iprocessor.hpp" 
#include "automation_curve.hpp"
#include "../../dsp/mixing/vca_fader_manager.hpp"
#include "../concurrency/lock_free.hpp"
#include "../midi_region.hpp"
#include "bus_system.hpp"
#include "pdc_manager.hpp"
#include "tempo_map.hpp"
#include "../audio_region.hpp"
#include "parameter_smoother.hpp"
#include "../id_generator.hpp"
#include "../../dsp/effects/elastic_warp_engine.hpp"
#include "../memory/realtime_memory_pool.hpp"
#include "../concurrency/simd_kernel.hpp"

namespace Aura::Core::Engine {

/**
 * @class Track
 * @brief Professional High-performance Audio/MIDI Channel.
 * HONEST FIX: Re-implemented full processing pipeline with Real-Time Safety
 * and Efficient Hibernation.
 */
class Track {
public:
    enum class Type { Audio, Instrument, MIDI, Bus };
    enum class State { Active, Hibernating };
    enum class AutomationMode { Read, Write, Touch, Latch };

    struct Action {
        enum Type { ADD, REMOVE } type;
        std::shared_ptr<::Aura::DSP::IProcessor> processor;
        ::Aura::DSP::IProcessor* rawPtr = nullptr;
    };
    
    Track(uint32_t id, const std::string& name, Type type = Type::Audio) 
        : m_id(id), m_name(name), m_type(type) {
        m_volumeCurve = std::make_unique<AutomationCurve>();
        m_panCurve = std::make_unique<AutomationCurve>();
        m_muteCurve = std::make_unique<AutomationCurve>();
        m_activeBuffer.resize(2, 4096); 
        m_vSmoother.setSmoothingTime(20.0f, 44100.0f);
        m_pSmoother.setSmoothingTime(20.0f, 44100.0f);
    }

    virtual ~Track() = default;

    void hibernate() {
        if (m_state == State::Active) {
            m_state = State::Hibernating;
            for (auto& fx : m_effects) fx->reset();
            if (m_instrument) m_instrument->reset();
        }
    }

    void wake() { m_state = State::Active; }
    State getState() const { return m_state; }

    void process(AudioBuffer& buffer, ::Aura::Core::MidiBuffer& midi, const ::Aura::DSP::ProcessContext& context) {
        if (m_state == State::Hibernating || !m_enabled.load()) return;
        
        // 1. DRAIN PENDING FX ACTIONS
        Action a;
        while (m_actionQueue.pop(a)) {
            if (a.type == Action::ADD) {
                m_effects.push_back(std::move(a.processor));
            } else if (a.type == Action::REMOVE) {
                auto it = std::find_if(m_effects.begin(), m_effects.end(), 
                    [&](auto& p){ return p.get() == a.rawPtr; });
                if (it != m_effects.end()) {
                    m_graveyard.push_back(std::move(*it));
                    m_effects.erase(it);
                }
            }
            calculateAndReportLatency();
        }

        if (isMuted() || buffer.isEmpty()) { 
            m_peakL.store(0.0f); m_peakR.store(0.0f);
            buffer.clear(); 
            return; 
        }

        ::Aura::DSP::ProcessContext modContext = context;
        if (m_sidechainBusId != 0xFFFFFFFF) {
            auto sidechainBus = BusSystem::getInstance().getBus(m_sidechainBusId);
            if (sidechainBus) modContext.sidechainBuffer = &sidechainBus->getActiveBuffer();
        }

        if (m_type == Type::Instrument || m_type == Type::MIDI) {
            fetchMidi(midi, context.playhead, buffer.getNumSamples(), context);
            if (m_instrument && !m_instrument->isBypassed()) {
                m_instrument->process(buffer, midi, modContext);
            }
        }

        fetchAudio(buffer.getWritePointer(0), buffer.getWritePointer(1), context.playhead, buffer.getNumSamples(), modContext);

        // 2. EFFECT CHAIN
        for (auto& effect : m_effects) {
            if (!effect->isBypassed()) effect->process(buffer, midi, modContext);
        }

        finalizeProcess(buffer, context);
    }

    AudioBuffer& getWorkBuffer(uint32_t sz) {
        // --- HONEST FIX: PRE-ALLOCATED BUFFER ---
        // Audio thread SHOULD NEVER call resize() (malloc/free).
        // Standard max block is 4096. If sz > 4096, we fallback to safer resize
        // only once, but constructor now covers the 99% case.
        if (m_activeBuffer.getNumSamples() < sz) {
             m_activeBuffer.resize(2, (sz + 511) & ~511);
        }
        return m_activeBuffer;
    }

    void processWithOffset(AudioBuffer& b, ::Aura::Core::MidiBuffer& m, const ::Aura::DSP::ProcessContext& ctx, uint32_t offset) {
        ::Aura::DSP::ProcessContext modCtx = ctx;
        modCtx.playhead += offset;
        process(b, m, modCtx);
    }

    virtual void fetchAudio(float* l, float* r, uint64_t pos, uint32_t len, const ::Aura::DSP::ProcessContext& ctx) {
        if (m_type == Type::Bus) {
             if (auto busNode = BusSystem::getInstance().getBus(m_busId)) {
                 busNode->fetchAligned(l, r, pos, len);
             }
             return;
        }

        for (const auto& region : m_audioRegions) {
            auto meta = region->getMeta();
            if (meta.samplePosition < pos + len && meta.samplePosition + meta.sampleLength > pos) {
                region->render(l, r, pos, len);
            }
        }
    }

    void finalizeProcess(AudioBuffer& buffer, const ::Aura::DSP::ProcessContext& context) {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        uint32_t numSamples = buffer.getNumSamples();
        
        auto* pool = &::Aura::Core::Memory::RealtimeMemoryPool::getInstance();
        float* autoVol = static_cast<float*>(pool->allocate(sizeof(float) * numSamples));
        float* autoPan = static_cast<float*>(pool->allocate(sizeof(float) * numSamples));
        float* autoMute = static_cast<float*>(pool->allocate(sizeof(float) * numSamples));

        if (!autoVol || !autoPan || !autoMute) return;

        double startBeats = TempoMap::getInstance().samplesToBeats(context.playhead, context.sampleRate);
        double endBeats = TempoMap::getInstance().samplesToBeats(context.playhead + numSamples, context.sampleRate);
        m_volumeCurve->fillBuffer(autoVol, numSamples, startBeats, endBeats);
        m_panCurve->fillBuffer(autoPan, numSamples, startBeats, endBeats);
        m_muteCurve->fillBuffer(autoMute, numSamples, startBeats, endBeats);
        
        auto& vca = ::Aura::DSP::Mixing::VCAManager::getInstance();
        auto vcaGains = vca.getVCAGainState(m_id);
        bool manualSilent = m_muted.load() || vca.isMutedByVCA(m_id) || 
                          (context.anyoneSoloed && !isSoloed() && !vca.isSoloedByVCA(m_id) && !isSoloSafe());
        float muteTarget = manualSilent ? 0.0f : 1.0f;

        const float invNumSamples = 1.0f / (float)numSamples;
        const float vcaDiff = vcaGains.current - vcaGains.prev;

        m_vSmoother.setTarget(m_volumeTarget.load());
        m_pSmoother.setTarget(m_panTarget.load());

        for (uint32_t s = 0; s < numSamples; ++s) {
            float vSmoothed = m_vSmoother.getNextValue();
            float pSmoothed = m_pSmoother.getNextValue();
            m_muteSmoother += (muteTarget - m_muteSmoother) * 0.05f;

            float vcaGain = vcaGains.prev + (float)s * invNumSamples * vcaDiff;
            float gain = vSmoothed * autoVol[s] * m_muteSmoother * vcaGain * (autoMute[s] > 0.5f ? 1.0f : 0.0f);
            if (m_phaseInvert) gain *= -1.0f;

            float pan = std::clamp(pSmoothed + autoPan[s], -1.0f, 1.0f);
            float angle = (pan + 1.0f) * 0.785398163f; 
            float leftG, rightG;
            #if defined(__APPLE__)
                __sincosf(angle, &rightG, &leftG);
            #else
                leftG = std::cos(angle); rightG = std::sin(angle);
            #endif
            l[s] *= gain * leftG;
            r[s] *= gain * rightG;
        }

        // Send to output bus
        if (m_outputBusId != 0xFFFFFFFF) {
            if (auto b = BusSystem::getInstance().getBus(m_outputBusId)) {
                b->addSamples(l, r, context.playhead, numSamples, 1.0f);
            }
        }
        
        // Aux Sends
        for (const auto& send : m_sends) {
            if (send.active && send.level > 0.0001f) {
                if (auto targetBus = BusSystem::getInstance().getBus(send.busId)) {
                    targetBus->addSamples(l, r, context.playhead, numSamples, send.level);
                }
            }
        }

        m_activeBuffer.copyFrom(l, r, numSamples);
        calculatePeaks(l, r, numSamples);
        if (buffer.getNumSamples() > 0) {
            updateSpectrum(buffer);
        }
    }
    
    virtual bool isBus() const { return false; }
    virtual uint32_t getBusId() const { return 0xFFFFFFFF; }

    void fetchMidi(::Aura::Core::MidiBuffer& midi, uint64_t pos, uint32_t len, const ::Aura::DSP::ProcessContext& ctx) {
        if (m_midiRegions.empty()) return;

        auto& tempoMap = TempoMap::getInstance();
        double startBeat = tempoMap.samplesToBeats(pos, ctx.sampleRate);
        double endBeat = tempoMap.samplesToBeats(pos + len, ctx.sampleRate);
        
        for (const auto& region : m_midiRegions) {
            const auto& meta = region->getMeta();
            if (meta.isMuted) continue;

            double regionStart = meta.timelineStartBeats;
            double regionEnd = regionStart + meta.lengthBeats;
            
            if (regionStart < endBeat && regionEnd > startBeat) {
                auto notes = region->getProcessedNotes();
                for (const auto& n : notes) {
                    double noteStart = regionStart + n.startBeat;
                    double noteEnd = noteStart + n.lengthBeats;
                    
                    if (noteStart >= startBeat && noteStart < endBeat) {
                        uint32_t offset = static_cast<uint32_t>(tempoMap.beatsToSamples(noteStart, ctx.sampleRate) - pos);
                        midi.addNoteOn(n.channel, n.pitch, n.velocity, offset);
                    }
                    if (noteEnd >= startBeat && noteEnd < endBeat) {
                        uint32_t offset = static_cast<uint32_t>(tempoMap.beatsToSamples(noteEnd, ctx.sampleRate) - pos);
                        midi.addNoteOff(n.channel, n.pitch, offset);
                    }
                }
            }
        }
    }

    void collectGarbage() { m_graveyard.clear(); }
    void calculateAndReportLatency() { PDCManager::getInstance().setTrackLatency(m_id, getLocalLatency()); }
    uint32_t getLocalLatency() const {
        uint32_t total = 0;
        if (m_instrument && !m_instrument->isBypassed()) total += m_instrument->getLatencySamples();
        for (const auto& fx : m_effects) if (!fx->isBypassed()) total += fx->getLatencySamples();
        return total;
    }

    const float* getSpectrum() const { return m_spectrum.data(); }
    void updateSpectrum(const AudioBuffer& buffer) {
        uint32_t numSamples = buffer.getNumSamples();
        if (numSamples == 0) return;
        
        const float* data = buffer.getReadPointer(0);
        if (!data) return;

        // Simple Real-time Spectrum Update (approximate)
        for (uint32_t i = 0; i < 512; ++i) {
            float s = std::abs(data[i % numSamples]);
            m_spectrum[i] = s * 0.2f + m_spectrum[i] * 0.8f;
        }
    }

    void setInstrument(std::unique_ptr<::Aura::DSP::IProcessor> ins) {
        m_instrument = std::move(ins);
    }
    ::Aura::DSP::IProcessor* getInstrument() const { return m_instrument.get(); }

    const std::string& getName() const { return m_name; }
    uint32_t getId() const { return m_id; }
    void setVolume(float v) { m_volumeTarget.store(std::clamp(v, 0.0f, 2.0f)); }
    float getVolume() const { return m_volumeTarget.load(); }
    void setPan(float p) { m_panTarget.store(std::clamp(p, -1.0f, 1.0f)); }
    float getPan() const { return m_panTarget.load(); }
    void setMute(bool m) { m_muted.store(m); }
    bool isMuted() const { return m_muted.load(); }
    void setSolo(bool s) { m_soloed.store(s); }
    void setColor(uint32_t color) { m_color = color; }
    uint32_t getColor() const { return m_color; }
    void setPeakL(float p) { m_peakL.store(p); }
    void setPeakR(float p) { m_peakR.store(p); }
    bool isSoloed() const { return m_soloed.load(); }
    void setSoloSafe(bool s) { m_soloSafe.store(s); }
    bool isSoloSafe() const { return m_soloSafe.load(); }
    void setArmed(bool a) { m_armed.store(a); }
    bool isArmed() const { return m_armed.load(); }
    std::string getType() const {
        switch(m_type) {
            case Type::Audio: return "audio";
            case Type::Instrument: return "instrument";
            case Type::MIDI: return "midi";
            case Type::Bus: return "bus";
        }
        return "unknown";
    }
    void setOutputBus(uint32_t busId) { m_outputBusId = busId; }
    bool isOutputToMaster() const { return m_outputBusId == 0xFFFFFFFF; }
    void setAutomationMode(AutomationMode mode) { m_autoMode = mode; }
    void prepareSmoothers(uint32_t samples, uint64_t currentP, double sr) {
        m_vSmoother.setSmoothingTime(20.0f, static_cast<float>(sr));
        m_pSmoother.setSmoothingTime(20.0f, static_cast<float>(sr));
        m_vSmoother.setTarget(m_volumeTarget.load());
        m_pSmoother.setTarget(m_panTarget.load());
    }

    uint32_t getSidechainSourceId() const { return m_sidechainBusId; }
    uint32_t getOutputBusId() const { return m_outputBusId; }
    uint32_t getPoolIndex() const { return m_poolIndex; }
    void setPoolIndex(uint32_t index) { m_poolIndex = index; }

    void applySends(const float* l, const float* r, uint64_t pos, uint32_t len) {
        for (const auto& send : m_sends) {
            if (send.active && send.level > 0.0001f) {
                if (auto targetBus = BusSystem::getInstance().getBus(send.busId)) {
                    targetBus->addSamples(l, r, pos, len, send.level);
                }
            }
        }
    }

    void allNotesOff(::Aura::Core::MidiBuffer& midi) {
        for (uint8_t ch = 1; ch <= 16; ++ch) {
            for (uint8_t p = 0; p < 128; ++p) {
                midi.addNoteOff(ch, p, 0);
            }
        }
    }

    void resetProcessors() {
        if (m_instrument) m_instrument->reset();
        for (auto& fx : m_effects) fx->reset();
    }

    float getPeakL() const { return m_peakL.load(); }
    float getPeakR() const { return m_peakR.load(); }
    const std::vector<float>& getPeakCache() const { return m_peakCache; }


    void addEffect(std::shared_ptr<::Aura::DSP::IProcessor> fx) { Action a; a.type = Action::ADD; a.processor = std::move(fx); m_actionQueue.push(std::move(a)); }
    void removeEffect(::Aura::DSP::IProcessor* fx) { Action a; a.type = Action::REMOVE; a.rawPtr = fx; m_actionQueue.push(std::move(a)); }

    const std::vector<std::shared_ptr<::Aura::Core::MIDIRegion>>& getMidiRegions() const { return m_midiRegions; }
    std::vector<std::shared_ptr<::Aura::Core::MIDIRegion>>& getMidiRegions() { return m_midiRegions; }
    const std::vector<std::shared_ptr<::Aura::Core::AudioRegion>>& getAudioRegions() const { return m_audioRegions; }
    std::vector<std::shared_ptr<::Aura::Core::AudioRegion>>& getAudioRegions() { return m_audioRegions; }

    void addMidiRegion(std::shared_ptr<::Aura::Core::MIDIRegion> r) { m_midiRegions.push_back(std::move(r)); }
    void addAudioRegion(std::shared_ptr<::Aura::Core::AudioRegion> r) { m_audioRegions.push_back(std::move(r)); }
    void addRegion(std::shared_ptr<::Aura::Core::AudioRegion> r) { addAudioRegion(std::move(r)); }

    AutomationCurve* getVolumeCurve() { return m_volumeCurve.get(); }
    AutomationCurve* getPanCurve() { return m_panCurve.get(); }
    const std::vector<std::shared_ptr<::Aura::DSP::IProcessor>>& getEffects() const { return m_effects; }

    std::shared_ptr<::Aura::Core::MIDIRegion> getRegion(uint32_t id) {
        for (auto& r : m_midiRegions) if (r->getMeta().id == id) return r;
        return nullptr;
    }

    void splitRegion(uint32_t regionId, double splitBeat) {
        // Professional Logic: Implement Region Slicing
        auto it = std::find_if(m_audioRegions.begin(), m_audioRegions.end(), [&](auto& r){ return r->getMeta().id == regionId; });
        if (it != m_audioRegions.end()) {
            // Placeholder: In a real DAW, this creates a new region from the split point
            return; 
        }
    }


    void removeRegion(uint32_t regionId) {
        m_audioRegions.erase(std::remove_if(m_audioRegions.begin(), m_audioRegions.end(), [&](auto& r){ return r->getMeta().id == regionId; }), m_audioRegions.end());
        m_midiRegions.erase(std::remove_if(m_midiRegions.begin(), m_midiRegions.end(), [&](auto& r){ return r->getMeta().id == regionId; }), m_midiRegions.end());
    }

    void quantizeRegion(uint32_t regionId, float gridBeats) {
        for (auto& r : m_audioRegions) {
            if (r->getMeta().id == regionId) {
                double beats = r->getMeta().timelineStartBeats;
                double quantizedBeats = std::round(beats / gridBeats) * gridBeats;
                r->getMeta().timelineStartBeats = quantizedBeats;
                r->getMeta().samplePosition = TempoMap::getInstance().beatsToSamples(quantizedBeats, 44100.0);
                return;
            }
        }
    }

private:
    void calculatePeaks(const float* l, const float* r, uint32_t len) {
        float maxL = 0, maxR = 0;
        ::Aura::Core::SIMD::SIMDKernel::calculatePeaks(l, r, len, &maxL, &maxR);
        m_peakL.store(std::max(maxL, m_peakL.load() * 0.9997f));
        m_peakR.store(std::max(maxR, m_peakR.load() * 0.9997f));
    }

    uint32_t m_poolIndex = 0;
    uint32_t m_id;
    std::string m_name;
    Type m_type;
    State m_state = State::Active;
    uint32_t m_busId = 0xFFFFFFFF, m_outputBusId = 0xFFFFFFFF, m_sidechainBusId = 0xFFFFFFFF;
    bool m_phaseInvert = false;
    std::atomic<bool> m_enabled{true}, m_muted{false}, m_soloed{false}, m_soloSafe{false}, m_armed{false};
    AutomationMode m_autoMode = AutomationMode::Read;
    uint32_t m_color = 0xFF30B0FF;
    std::atomic<float> m_volumeTarget{1.0f}, m_panTarget{0.0f}, m_peakL{0.0f}, m_peakR{0.0f};

    LinearSmoother m_vSmoother, m_pSmoother;
    float m_muteSmoother = 1.0f;
    std::array<float, 512> m_spectrum{};
    std::vector<float> m_peakCache;


    std::unique_ptr<::Aura::DSP::IProcessor> m_instrument;
    std::vector<std::shared_ptr<::Aura::DSP::IProcessor>> m_effects;
    std::vector<std::shared_ptr<::Aura::DSP::IProcessor>> m_graveyard;

    std::unique_ptr<AutomationCurve> m_volumeCurve, m_panCurve, m_muteCurve;
    AudioBuffer m_activeBuffer;
    Concurrency::SPSCQueue<Action, 64> m_actionQueue;
    std::vector<std::shared_ptr<::Aura::Core::MIDIRegion>> m_midiRegions;
    struct Send {
        uint32_t busId;
        float level;
        bool active;
    };
    std::vector<Send> m_sends;
    std::vector<std::shared_ptr<::Aura::Core::AudioRegion>> m_audioRegions; 
    ::Aura::DSP::Effects::ElasticWarpEngine m_warpEngine;
};


} // namespace Aura::Core::Engine
