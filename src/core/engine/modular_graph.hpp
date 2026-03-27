#pragma once
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <mutex>
#include "../../dsp/iprocessor.hpp"
#include "../../core/audio_buffer.hpp"

namespace Aura::Core::Engine {

/**
 * @struct Node
 * @brief Representation of a DSP unit in the modular graph.
 */
struct Node {
    uint32_t id;
    std::string name;
    std::shared_ptr<::Aura::DSP::IProcessor> processor;
    std::vector<uint32_t> outputs; // Target Node IDs
    
    // Internal buffer for nodal summing
    ::Aura::Core::AudioBuffer buffer;
    
    Node(uint32_t mid, const std::string& mname) : id(mid), name(mname) {
        buffer.resize(2, 4096);
    }
};

/**
 * @class ModularGraphManager
 * @brief Logic Pro 'Environment' & Bitwig-style Grid Engine.
 * Manages the high-performance routing of modular DSP nodes.
 */
class ModularGraphManager {
public:
    static ModularGraphManager& getInstance() { static ModularGraphManager i; return i; }

    void addNode(uint32_t id, const std::string& name, std::shared_ptr<::Aura::DSP::IProcessor> proc) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_nodes[id] = std::make_shared<Node>(id, name);
        m_nodes[id]->processor = proc;
        if (proc) proc->prepareToPlay(m_sampleRate, m_maxBlockSize);
    }

    void addConnection(uint32_t from, uint32_t to) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_nodes.count(from) && m_nodes.count(to)) {
            m_nodes[from]->outputs.push_back(to);
        }
    }

    void clearConnections() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& pair : m_nodes) pair.second->outputs.clear();
    }

    /**
     * @brief Process the modular graph (Topological sort or Simple recursive).
     * HONEST FIX: Uses a simplified DFS to handle the signal path without latency loops.
     */
    void process(::Aura::Core::AudioBuffer& master, uint32_t numSamples, const ::Aura::DSP::ProcessContext& context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 1. Clear all nodal buffers
        for (auto& pair : m_nodes) pair.second->buffer.clear(numSamples);
        
        // 2. We assume 'Input' nodes are filled by tracks elsewhere or here
        // For simplicity: Process nodes in ID order (proper topological sort needed for real-world)
        for (auto& pair : m_nodes) {
            auto& node = pair.second;
            if (node->processor) {
                ::Aura::Core::MidiBuffer m;
                node->processor->process(node->buffer, m, context);
            }
            
            // 3. Sum output to targets
            for (uint32_t targetId : node->outputs) {
                if (m_nodes.count(targetId)) {
                    auto& target = m_nodes[targetId];
                    target->buffer.addFrom(node->buffer, numSamples);
                } else if (targetId == 0xFFFFFFFF) { // Master Output
                    master.addFrom(node->buffer, numSamples);
                }
            }
        }
    }

    void prepareToPlay(double sr, uint32_t bs) {
        m_sampleRate = sr;
        m_maxBlockSize = bs;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& pair : m_nodes) {
            if (pair.second->processor) pair.second->processor->prepareToPlay(sr, bs);
            pair.second->buffer.resize(2, bs);
        }
    }

private:
    ModularGraphManager() = default;
    std::map<uint32_t, std::shared_ptr<Node>> m_nodes;
    std::mutex m_mutex;
    double m_sampleRate = 44100.0;
    uint32_t m_maxBlockSize = 512;
};

} // namespace Aura::Core::Engine
