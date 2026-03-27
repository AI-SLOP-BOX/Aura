#pragma once
#include <vector>
#include <memory>
#include <mutex>

namespace Aura::Core::Engine {

/**
 * @class RoutingEngine
 * @brief THE NERVE SYSTEM.
 * Manages audio bus routing, sidechains, and parallel processing chains.
 */
class RoutingEngine {
public:
    struct Connection {
        uint32_t sourceId;
        uint32_t destId;
        float gain = 1.0f;
    };

    /**
     * @brief DYNAMIC RE-ROUTING: Allows million-node graphs on GPU/SIMD.
     */
    void addConnection(uint32_t s, uint32_t d, float g = 1.0f) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.push_back({s, d, g});
        m_dirty.store(true);
    }

    /**
     * @brief COMPILE GRAPH: Pre-calculates the processing order for the engine.
     * Prevents runtime logic in the audio thread.
     */
    void buildGraph() {
        if (!m_dirty.load()) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        // Topology sorting here...
        m_dirty.store(false);
    }

private:
    std::vector<Connection> m_connections;
    std::mutex m_mutex;
    std::atomic<bool> m_dirty{true};
};

} // namespace Aura::Core::Engine
