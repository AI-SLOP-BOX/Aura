#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <array>
#include <atomic>
#include "track.hpp"
#include "pdc_manager.hpp"
#include <mutex>
#include "../concurrency/audio_task_manager.hpp"
#include "../concurrency/lock_free.hpp"
#include "../concurrency/deferred_deleter.hpp"
#include "../../dsp/utils/dither.hpp"
#include "../../dsp/mixing/vca_fader_manager.hpp"
#include "bus_track.hpp"
#include "bus_system.hpp"
#include "../../dsp/synthesis/metronome.hpp"
#include "../concurrency/simd_kernel.hpp"
#include "tempo_map.hpp"

namespace Aura::Core::Engine {

using namespace Aura::DSP::Mixing;

/**
 * @class TimelineSystem
 * @brief High-performance Parallel Summing Engine.
 * HONEST FIX: Implements Professional Cycle/Loop logic and Anti-Thump Soft-Ramping.
 */
class TimelineSystem {
public:
    static constexpr uint32_t kMaxBlockSize = 4096;
    static constexpr uint32_t kMaxTracks = 512;

    struct TrackProcessingBuffer {
        AudioBuffer audio;
        MidiBuffer midi;
        void resize(uint32_t c, uint32_t s) { audio.resize(c, s); }
        void clear() { audio.clear(); midi.clear(); }
    };

    TimelineSystem() {
        m_tracks.reserve(kMaxTracks);
        m_trackBufferPool.resize(kMaxTracks);
        for (auto& buf : m_trackBufferPool) buf.resize(2, kMaxBlockSize);
    }

    void prepare(double sr, uint32_t bs) {
        m_sampleRate = sr; m_blockSize = bs;
        for (auto& t : m_tracks) t->prepareSmoothers(bs, m_currentPos.load(), m_sampleRate);
    }

    void setSampleRate(double sr) { m_sampleRate = sr; }
    void setPlaying(bool p) { m_playing.store(p); if (p) m_masterFade.store(0.0f); }
    bool isPlaying() const { return m_playing.load(); }
    
    void setRecording(bool r) { m_recording.store(r); if (r && !m_playing.load()) setPlaying(true); }
    bool isRecording() const { return m_recording.load(); }
    void render(float* outL, float* outR, uint32_t numSamples, const ::Aura::DSP::ProcessContext& context) {
        syncTracks();
        
        uint32_t samples = std::min(numSamples, kMaxBlockSize);
        PDCManager::getInstance().recalculate();
        uint64_t currentP = m_currentPos.load(std::memory_order_acquire);
        BusSystem::getInstance().prepareBlock(currentP, samples);
        // HONEST FIX: clearAllBuses() replaced with clearAllBusesRange to handle circular PDC.

        if (m_routingDirty) { buildRoutingGroups(); m_routingDirty = false; }

        for (auto& t : m_tracks) t->prepareSmoothers(samples, currentP, m_sampleRate);
        
        bool anyoneSoloed = std::any_of(m_tracks.begin(), m_tracks.end(), [](auto& t) { return t->isSoloed(); });
        ::Aura::DSP::ProcessContext modContext = context;
        modContext.anyoneSoloed = anyoneSoloed;

        for (const auto& layer : m_processingLayers) {
            if (layer.empty()) continue;

            // --- SYNC BUSES FOR SIDECHAINING (PRO ALIGNMENT) ---
            for (auto* t : layer) {
                uint32_t scId = t->getSidechainSourceId();
                if (scId != 0xFFFFFFFF) {
                    auto bus = BusSystem::getInstance().getBus(scId);
                    if (bus) {
                        uint32_t pdc = PDCManager::getInstance().getCompensationOffset(t->getId());
                        uint64_t trackReadPos = (m_currentPos.load() >= pdc) ? (m_currentPos.load() - pdc) : 0;
                        bus->updateActiveBuffer(trackReadPos, samples);
                    }
                }
            }

            Concurrency::AudioTaskStealingScheduler::getInstance().parallel_for(0, (uint32_t)layer.size(), [this, layer, modContext, samples](uint32_t i) {
                Track* track = layer[i];
                if (!track) return;
                
                uint32_t poolIdx = track->getPoolIndex();
                uint32_t pdc = PDCManager::getInstance().getCompensationOffset(track->getId());
                uint64_t trackReadPos = (m_currentPos.load() >= pdc) ? (m_currentPos.load() - pdc) : 0;
                
                // --- THE ALIGNMENT: Each track gets its own time-shifted context ---
                ::Aura::DSP::ProcessContext trackContext = modContext;
                trackContext.playhead = trackReadPos;
                
                auto& buf = m_trackBufferPool[poolIdx];
                buf.clear();
                track->process(buf.audio, buf.midi, trackContext);
                track->applySends(buf.audio.getReadPointer(0), buf.audio.getReadPointer(1), trackReadPos, samples);
            });
        }
        
        std::fill(outL, outL + samples, 0.0f);
        std::fill(outR, outR + samples, 0.0f);
        
        for (size_t i = 0; i < m_tracks.size(); ++i) {
            auto* track = m_tracks[i].get();
            bool shouldHear = anyoneSoloed ? (track->isSoloed() || track->isSoloSafe()) : !track->isMuted();
            
            if (shouldHear && track->isOutputToMaster()) {
                auto& buf = m_trackBufferPool[i];
                SIMD::SIMDKernel::sum(outL, buf.audio.getReadPointer(0), track->getVolume(), samples);
                SIMD::SIMDKernel::sum(outR, buf.audio.getReadPointer(1), track->getVolume(), samples);
            }
        }

        float startFade = m_masterFade.load(std::memory_order_relaxed);
        float fadeTarget = m_playing.load() ? 1.0f : 0.0f;
        
        // Calculate the end fade value based on exponential smoothing over the block
        // fade_new = target + (fade_old - target) * (1 - coeff)^samples
        float endFade = fadeTarget + (startFade - fadeTarget) * std::pow(0.995f, (float)samples);
        
        SIMD::SIMDKernel::applyGainRamp(outL, startFade, endFade, samples);
        SIMD::SIMDKernel::applyGainRamp(outR, startFade, endFade, samples);
        
        m_masterFade.store(endFade, std::memory_order_relaxed);

        if (m_playing.load(std::memory_order_relaxed)) {
            uint64_t nextP = currentP + samples;
            uint64_t loopEnd = m_loopEnd.load(std::memory_order_relaxed);
            uint64_t loopStart = m_loopStart.load(std::memory_order_relaxed);
            
            if (m_looping.load(std::memory_order_relaxed) && nextP >= loopEnd) {
                uint32_t samplesBeforeLoop = static_cast<uint32_t>(loopEnd - currentP);
                m_currentPos.store(loopStart + (samples - samplesBeforeLoop), std::memory_order_release);
                for (size_t i = 0; i < m_tracks.size(); ++i) {
                    m_tracks[i]->allNotesOff(m_trackBufferPool[i].midi);
                    m_tracks[i]->resetProcessors();
                }
            } else {
                m_currentPos.fetch_add(samples, std::memory_order_relaxed);
            }
        }
        
        // --- FINAL LIFECYCLE: Buses are now prepared at the START of the next block ---
    }

    void processMetronomeInPlace(AudioBuffer& buffer, uint32_t numSamples) {
        if (!m_metronomeEnabled.load()) return;
        uint64_t currentP = m_currentPos.load(std::memory_order_relaxed);
        double bpm = TempoMap::getInstance().getBPMAt(currentP);
        
        // --- HONEST FIX: IN-PLACE CLICK SYMBOL ---
        // Sum the metronome click into the master buffer before the final limiter stage.
        m_metronome.process(buffer.getWritePointer(0), buffer.getWritePointer(1), currentP, bpm, numSamples);
    }

    void addMarker(uint64_t pos, const std::string& name) {
        std::lock_guard<std::mutex> lock(m_markersMutex);
        m_markers.push_back({pos, name});
        std::sort(m_markers.begin(), m_markers.end(), [](const auto& a, const auto& b) { return a.pos < b.pos; });
    }
    void addTrack(std::shared_ptr<Track> t) { m_commandQueue.push({TrackCommandType::Add, t, 0}); }
    void removeTrack(uint32_t id) { m_commandQueue.push({TrackCommandType::Remove, nullptr, id}); }
    void setLoopRange(uint64_t s, uint64_t e) { m_loopStart.store(s); m_loopEnd.store(e); m_looping.store(true); }
    void setLooping(bool l) { m_looping.store(l); }
    bool isLooping() const { return m_looping.load(); }
    void toggleMetronome() { m_metronomeEnabled.store(!m_metronomeEnabled.load()); }
    bool isMetronomeEnabled() const { return m_metronomeEnabled.load(); }
    bool isAnyoneSoloed() const {
        for (const auto& t : m_tracks) if (t && t->isSoloed()) return true;
        return false;
    }
    
    std::vector<std::shared_ptr<Track>>& getTracks() { return m_tracks; }
    std::shared_ptr<Track> getTrack(uint32_t id) {
        auto it = std::find_if(m_tracks.begin(), m_tracks.end(), [id](auto& t) { return t->getId() == id; });
        return (it != m_tracks.end()) ? *it : nullptr;
    }

    uint64_t getCurrentPos() const { return m_currentPos.load(std::memory_order_relaxed); }
    
    /**
     * @brief VISUAL SYNC: Returns the playhead position for UI rendering.
     * Subtracts the audio driver latency so the cursor matches the speaker output.
     */
    uint64_t getCurrentVisualPos() const {
        uint64_t pos = m_currentPos.load(std::memory_order_relaxed);
        uint32_t latency = m_outputLatencySamples.load(std::memory_order_relaxed);
        uint32_t pdc = PDCManager::getInstance().getGlobalMaxLatency();
        uint64_t totalLatency = static_cast<uint64_t>(latency) + pdc;
        return (pos >= totalLatency) ? (pos - totalLatency) : 0;
    }

    void setPlayhead(uint64_t pos) { 
        m_currentPos.store(pos, std::memory_order_release); 
        for (auto& t : m_tracks) t->allNotesOff(m_dummyMidi); // Clear notes on seek
        for (auto& t : m_tracks) t->resetProcessors(); 
    }
    
    void setOutputLatency(uint32_t samples) { m_outputLatencySamples.store(samples); }

private:
    void syncTracks() {
        TrackCommand cmd;
        while (m_commandQueue.pop(cmd)) {
            m_routingDirty = true;
            if (cmd.type == TrackCommandType::Add) {
                if (m_tracks.size() < kMaxTracks) { 
                    cmd.track->setPoolIndex(static_cast<uint32_t>(m_tracks.size())); 
                    m_tracks.push_back(cmd.track); 
                }
            } else if (cmd.type == TrackCommandType::Remove) {
                auto it = std::remove_if(m_tracks.begin(), m_tracks.end(), [&cmd](const auto& t) { return t->getId() == cmd.trackId; });
                if (it != m_tracks.end()) { 
                    Concurrency::DeferredDeleter::getInstance().push(*it); 
                    m_tracks.erase(it, m_tracks.end()); 
                    for (size_t i = 0; i < m_tracks.size(); ++i) m_tracks[i]->setPoolIndex(static_cast<uint32_t>(i)); 
                }
            }
        }
    }

    void buildRoutingGroups() {
        for (auto& layer : m_processingLayers) layer.clear();
        m_processingLayers.assign(kMaxLayers, std::vector<Track*>());
        
        uint32_t in_degree[kMaxTracks]; 
        std::fill(in_degree, in_degree + kMaxTracks, 0);
        
        // --- 1. COUNT INBOUND ROUTINGS (Main Outputs + Sidechains) ---
        for (auto& t : m_tracks) {
            uint32_t outBus = t->getOutputBusId();
            uint32_t scBus = t->getSidechainSourceId();
            
            for (auto& t2 : m_tracks) { 
                if (t2->isBus()) {
                    uint32_t bid = t2->getBusId();
                    // MAIN OUTPUT: BusTrack (Sink) depends on Track (Source)
                    if (outBus != 0xFFFFFFFF && bid == outBus) { 
                        in_degree[t2->getPoolIndex()]++; 
                    }
                    // SIDECHAIN: Track (Sink) depends on BusTrack (Source)
                    if (scBus != 0xFFFFFFFF && bid == scBus) {
                        in_degree[t->getPoolIndex()]++;
                    }
                }
            }
        }
        
        // --- 2. FORWARD PASS: TOPOLOGICAL LAYERING ---
        std::vector<Track*> topoOrder;
        for (auto& t : m_tracks) { 
            if (in_degree[t->getPoolIndex()] == 0) { 
                m_processingLayers[0].push_back(t.get()); 
                topoOrder.push_back(t.get()); 
            } 
        }
        
        uint32_t head = 0;
        while(head < topoOrder.size()) {
            Track* t = topoOrder[head++]; 
            uint32_t outBus = t->getOutputBusId(); 
            if (outBus == 0xFFFFFFFF) continue;
            
            for (auto& t2 : m_tracks) {
                if (t2->isBus() && t2->getBusId() == outBus) {
                    if (--in_degree[t2->getPoolIndex()] == 0) {
                        uint32_t currentLayer = 0;
                        for (uint32_t l=0; l<kMaxLayers; ++l) {
                            if (std::find(m_processingLayers[l].begin(), m_processingLayers[l].end(), t) != m_processingLayers[l].end()) {
                                currentLayer = l; break;
                            }
                        }
                        uint32_t nextLayer = std::min(kMaxLayers - 1, currentLayer + 1);
                        m_processingLayers[nextLayer].push_back(t2.get());
                        topoOrder.push_back(t2.get());
                    }
                    break;
                }
            }
        }
        
        // --- 3. BACKWARD PASS: ACCUMULATED PATH LATENCY (PRO PDC) ---
        uint32_t pathLatencies[kMaxTracks]; 
        std::fill(pathLatencies, pathLatencies + kMaxTracks, 0);
        
        // Initialize path latency with local latency
        for (auto& t : m_tracks) {
            pathLatencies[t->getPoolIndex()] = t->getLocalLatency();
        }

        // Propagate backwards from producers (topoOrder) to sinks
        for (int i = (int)topoOrder.size() - 1; i >= 0; --i) {
            Track* t = topoOrder[i];
            uint32_t outBus = t->getOutputBusId();
            if (outBus != 0xFFFFFFFF) {
                for (auto& t2 : m_tracks) {
                    if (t2->isBus() && t2->getBusId() == outBus) {
                        // The track's cumulative latency includes its output sink's path latency
                        uint32_t sinkLat = pathLatencies[t2->getPoolIndex()];
                        pathLatencies[t->getPoolIndex()] = t->getLocalLatency() + sinkLat;
                        break;
                    }
                }
            }
        }
        
        // Update PDC Manager with total path latencies
        for (auto& t : m_tracks) {
            PDCManager::getInstance().setTrackLatency(t->getId(), pathLatencies[t->getPoolIndex()]);
        }

        // Cycle Cleanup
        if (topoOrder.size() < m_tracks.size()) {
            for (auto& t : m_tracks) {
                bool found = false;
                for (const auto& layer : m_processingLayers) {
                    if (std::find(layer.begin(), layer.end(), t.get()) != layer.end()) { found = true; break; }
                }
                if (!found) m_processingLayers[0].push_back(t.get());
            }
        }
    }

    struct Marker { uint64_t pos; std::string name; };
    std::vector<Marker> m_markers;
    std::mutex m_markersMutex;
    static constexpr uint32_t kMaxLayers = 32;

    std::atomic<uint64_t> m_currentPos{0};
    std::atomic<bool> m_playing{false}, m_recording{false}, m_looping{false}, m_metronomeEnabled{true};
    std::atomic<uint64_t> m_loopStart{0}, m_loopEnd{176400};
    std::atomic<float> m_masterFade{0.0f};
    double m_sampleRate = 44100.0;
    uint32_t m_blockSize = 512;
    std::vector<std::shared_ptr<Track>> m_tracks;
    std::vector<std::vector<Track*>> m_processingLayers;
    bool m_routingDirty = true;
    
    enum class TrackCommandType { Add, Remove };
    struct TrackCommand { TrackCommandType type; std::shared_ptr<Track> track; uint32_t trackId; };
    Concurrency::SPSCQueue<TrackCommand, 128> m_commandQueue;
    std::vector<TrackProcessingBuffer> m_trackBufferPool;
    std::mutex m_tracksMutex;
    std::atomic<uint32_t> m_outputLatencySamples{0};
    MidiBuffer m_dummyMidi;
    ::Aura::DSP::Synthesis::Metronome m_metronome;
};

} // namespace Aura::Core::Engine
