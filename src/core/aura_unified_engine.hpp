#pragma once
#include "concurrency/audio_task_manager.hpp"
#include "engine/track.hpp"
#include "concurrency/lock_free_command_queue.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <algorithm>
#include "musical_time.hpp"
#include "concurrency/lock_free.hpp"
#include "engine/engine_clock.hpp"

#include "engine/metronome.hpp"
#include "gpu_audio_kernel.hpp"

#include "../dsp/analysis/master_meter.hpp"
#include "../dsp/analysis/analysis_engine.hpp"
#include "../dsp/analysis/loudness_analyzer.hpp"
#include "../scae/AuraAISuite.hpp"
#include "concurrency/status_queue.hpp"
#include "engine/auto_save_manager.hpp"
#include "memory/realtime_memory_pool.hpp"
#include "engine/live_loops_engine.hpp"
#include "engine/drummer_engine.hpp"
#include "engine/modular_graph.hpp"
#include "delta_state_manager.hpp"

namespace Aura::Core::Engine {

class AuraUnifiedEngine {
    friend class SmartPurgeManager;
public:
    static AuraUnifiedEngine& getInstance() { static AuraUnifiedEngine i; return i; }
    
    std::vector<std::shared_ptr<Track>> getTracksSafe() const {
        std::lock_guard<std::mutex> lock(m_tracksMutex);
        return m_tracks;
    }
    
    std::vector<std::shared_ptr<Track>>& getTracks() { return m_tracks; }

    void addTrack(std::shared_ptr<Track> track) {
        m_structuralCommandQueue.push({StructuralCommand::ADD, track, 0});
    }

    void removeTrack(uint32_t trackId) {
        m_structuralCommandQueue.push({StructuralCommand::REMOVE, nullptr, trackId});
    }

    void prepareToPlay(double sampleRate, uint32_t samplesPerBlock) {
        m_context.sampleRate = sampleRate;
        m_context.blockSize = samplesPerBlock;
        m_masterBuffer.resize(2, samplesPerBlock);
        EngineClock::getInstance().setSampleRate(sampleRate);
    }

    void process(AudioBuffer& master, uint32_t numSamples, const ::Aura::DSP::ProcessContext& context) {
        // --- 1. SWITCH TO NEXT STABLE ARENA ---
        auto* pool = &Memory::RealtimeMemoryPool::getInstance();
        pool->nextFrame();
        syncStructuralChanges();
        
        // --- 2. STACK-LOCAL TRACK MANAGEMENT (Safety First) ---
        uint32_t totalTracks = static_cast<uint32_t>(m_tracks.size() + 32); 
        Track** activeTracks = static_cast<Track**>(pool->allocate(sizeof(Track*) * totalTracks));
        Track** activeBuses = static_cast<Track**>(pool->allocate(sizeof(Track*) * totalTracks));
        if (!activeTracks || !activeBuses) return;

        uint32_t activeTrackCount = 0;
        uint32_t activeBusCount = 0;
        {
            // PERFORMANCE: No more vector copying (m_audioFrameTracks = m_tracks).
            // We use the mutex as a simple read-lock but avoid the expensive deep clone
            // of the shared_ptrs by storing raw pointers for the duration of the process() call.
            std::lock_guard<std::mutex> lock(m_tracksMutex);
            for (auto& t : m_tracks) {
                if (t->isBus()) activeBuses[activeBusCount++] = t.get();
                else activeTracks[activeTrackCount++] = t.get();
            }
        }


        ::Aura::DSP::Mixing::VCAManager::getInstance().syncVCAs();

        m_context.playhead = context.playhead;
        m_context.isPlaying = context.isPlaying;
        m_context.anyoneSoloed = m_anyoneSoloedCache.load(std::memory_order_relaxed);

        uint32_t samplesProcessed = 0;
        while (samplesProcessed < numSamples) {
            uint32_t subBlockSize = std::min(numSamples - samplesProcessed, (uint32_t)1024);
            
            if (m_context.isLooping && m_context.playhead < m_context.cycleEnd) {
                uint32_t dist = (uint32_t)(m_context.cycleEnd - m_context.playhead);
                if (subBlockSize > dist) subBlockSize = dist;
            }

            LiveLoopsEngine::getInstance().update(m_context.playhead + samplesProcessed, m_context.sampleRate);
            processSubBlock(master, samplesProcessed, subBlockSize, activeTracks, activeTrackCount, activeBuses, activeBusCount);
            ModularGraphManager::getInstance().process(master, subBlockSize, m_context);
            
            samplesProcessed += subBlockSize;
            m_context.playhead += subBlockSize;
            if (m_context.isLooping && m_context.playhead >= m_context.cycleEnd) m_context.playhead = m_context.cycleStart;
        }

        if (m_aiAssistEnabled) {
            if (!m_analyzer) m_analyzer = std::make_unique<::Aura::DSP::Analysis::LoudnessAnalyzer>(m_context.sampleRate);
            if (m_context.playhead % 4096 == 0) { // Throttled update
                m_analyzer->process(master.getReadPointer(0), master.getReadPointer(1), numSamples);
            }

            calculateFFT(master, numSamples);
        }
        EngineClock::getInstance().setPlayhead(m_context.playhead);
    }

    void processSubBlock(AudioBuffer& output, uint32_t offset, uint32_t size, Track** activeTracks, uint32_t trackCount, Track** activeBuses, uint32_t busCount) {
        auto& scheduler = Concurrency::AudioTaskStealingScheduler::getInstance();
        m_masterBuffer.clear(size);

        auto trackProc = [](uint32_t i, void* data, Track** tList, uint32_t off, uint32_t sz) {
            auto* self = static_cast<AuraUnifiedEngine*>(data);
            tList[i]->processWithOffset(tList[i]->getWorkBuffer(sz), self->m_blockMidi, self->m_context, off);
        };

        scheduler.parallel_for_with_data(0, trackCount, trackProc, this, activeTracks, offset, size);
        scheduler.parallel_for_with_data(0, busCount, trackProc, this, activeBuses, offset, size);

        uint32_t maxLatency = PDCManager::getInstance().getMaxLatency();
        for (uint32_t i = 0; i < trackCount; ++i) {
            if (activeTracks[i]->isOutputToMaster()) {
                const auto& b = activeTracks[i]->getWorkBuffer(size);
                GPU::MetalAudioKernel::getInstance().sumBuffers(m_masterBuffer.getWritePointer(0), b.getReadPointer(0), size);
                GPU::MetalAudioKernel::getInstance().sumBuffers(m_masterBuffer.getWritePointer(1), b.getReadPointer(1), size);
            }
        }
        sumToOutput(m_masterBuffer, output, m_masterGain.load(), offset, size);
    }

    void calculateFFT(const AudioBuffer& buffer, uint32_t numSamples) {
        for (int i = 0; i < 64; ++i) {
            m_fftBands[i] = std::abs(buffer.getReadPointer(0)[i % numSamples]) * 0.5f + m_fftBands[i] * 0.5f;
        }
    }

    void sumToOutput(AudioBuffer& src, AudioBuffer& dst, float gain, uint32_t offset, uint32_t size) {
        for(int c=0; c<2; ++c) {
            float* d = dst.getWritePointer(c) + offset;
            const float* s = src.getReadPointer(c);
            for(uint32_t i=0; i<size; ++i) d[i] += s[i] * gain;
        }
    }

    void clearAllTracks() { std::lock_guard<std::mutex> lock(m_tracksMutex); m_tracks.clear(); }
    void syncStructuralChanges() {
        StructuralCommand cmd;
        while (m_structuralCommandQueue.pop(cmd)) {
            std::lock_guard<std::mutex> lock(m_tracksMutex);
            if (cmd.type == StructuralCommand::ADD) m_tracks.push_back(cmd.track);
            else if (cmd.type == StructuralCommand::REMOVE) {
                m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(), [&](const auto& t){ return t->getId() == cmd.id; }), m_tracks.end());
            }
        }
    }

    std::shared_ptr<Track> getTrack(uint32_t id) {
        std::lock_guard<std::mutex> lock(m_tracksMutex);
        for (auto& t : m_tracks) if (t->getId() == id) return t;
        return nullptr;
    }

private:
    struct StructuralCommand { enum { ADD, REMOVE } type; std::shared_ptr<Track> track; uint32_t id; };
    std::vector<std::shared_ptr<Track>> m_tracks;

    mutable std::mutex m_tracksMutex;
    Concurrency::SPSCQueue<StructuralCommand, 32> m_structuralCommandQueue;
    ::Aura::DSP::ProcessContext m_context;
    AudioBuffer m_masterBuffer;
    MidiBuffer m_blockMidi;
    std::atomic<float> m_masterGain{1.0f};
    std::array<float, 64> m_fftBands{};
    std::atomic<bool> m_anyoneSoloedCache{false};
    bool m_aiAssistEnabled = true;
    std::unique_ptr<::Aura::DSP::Analysis::LoudnessAnalyzer> m_analyzer;
};

} // namespace Aura::Core::Engine
