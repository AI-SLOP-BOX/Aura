#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <algorithm>

#include "audio_buffer.hpp"
#include "midi_buffer.hpp"
#include "engine/bus_system.hpp"
#include "../dsp/iprocessor.hpp"
#include "../dsp/effects/delay_line.hpp"

namespace Aura::Core {

/**
 * @class AudioProcessorGraph
 * @brief Orchestrates a dynamic chain of IProcessors with automatic Parallel Blending.
 */
class AudioProcessorGraph {
public:
    using IProcessor = DSP::IProcessor;

    AudioProcessorGraph() = default;

    void prepare(double sr, uint32_t bs) {
        m_sampleRate = sr;
        m_maxBlockSize = bs;
        m_dryBuffer.resize(2, bs); 
        m_dryDelayLines.clear();
        for (int i = 0; i < 2; ++i) m_dryDelayLines.emplace_back(65536);
        for (auto& node : m_nodes) node->prepareToPlay(sr, bs);
    }

    void addNode(std::shared_ptr<IProcessor> node) {
        m_nodes.push_back(node);
        if (m_sampleRate > 0) node->prepareToPlay(m_sampleRate, m_maxBlockSize);
    }

    /**
     * @brief THE ENGINE ROOM: Processes the entire plugin graph.
     */
    void process(AudioBuffer& buffer, MidiBuffer& midi, const DSP::ProcessContext& context) {
        for (auto& node : m_nodes) {
            if (node->isBypassed()) continue;

            float mix = node->getMix();
            uint32_t latency = node->getLatencySamples();
            
            if (mix < 1.0f) {
                for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
                    float* dry = m_dryBuffer.getWritePointer(c);
                    const float* src = buffer.getReadPointer(c);
                    uint32_t samples = buffer.getNumSamples();

                    if (latency > 0) {
                        for (uint32_t s = 0; s < samples; ++s) {
                            dry[s] = m_dryDelayLines[c].process(src[s], latency);
                        }
                    } else {
                        std::copy(src, src + samples, dry);
                    }
                }
            }

            node->process(buffer, midi, context);

            if (mix < 1.0f) {
                for (uint32_t c = 0; c < buffer.getNumChannels(); ++c) {
                    float* wet = buffer.getWritePointer(c);
                    const float* dry = m_dryBuffer.getReadPointer(c);
                    for (uint32_t s = 0; s < buffer.getNumSamples(); ++s) {
                        wet[s] = (wet[s] * mix) + (dry[s] * (1.0f - mix));
                    }
                }
            }
        }
    }

    uint32_t getTotalLatency() const {
        uint32_t total = 0;
        for (const auto& node : m_nodes) total += node->getLatencySamples();
        return total;
    }

    void reset() { for (auto& node : m_nodes) node->reset(); }
    void clear() { 
        // FIX: NEVER delete directly from the UI or Audio thread.
        // Use the DeferredDeleter to safely trash the shared pointers.
        for (auto& node : m_nodes) {
            Concurrency::DeferredDeleter::getInstance().push(std::move(node));
        }
        m_nodes.clear(); 
    }

private:
    std::vector<std::shared_ptr<IProcessor>> m_nodes;
    AudioBuffer m_dryBuffer;
    std::vector<DSP::Effects::DelayLine> m_dryDelayLines; 
    double m_sampleRate = 44100.0;
    uint32_t m_maxBlockSize = 512;
};

} // namespace Aura::Core
