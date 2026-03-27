#pragma once

#include <vector>
#include <memory>
#include <string>

namespace Aura::Core::Engine {

/**
 * @brief ImmersiveBus: A multi-channel summing lane for Atmos/Surround.
 * Supports up to 12 channels (L, R, C, LFE, Ls, Rs, Lb, Rb, 4xHeight).
 */
class ImmersiveBus {
public:
    ImmersiveBus(uint32_t id, const std::string& name, uint32_t numChannels = 12) 
        : m_id(id), m_name(name), m_numChannels(numChannels) {
        m_buffers.resize(numChannels);
        for (auto& b : m_buffers) b.resize(1024, 0.0f);
    }

    void clear() {
        for (auto& b : m_buffers) std::fill(b.begin(), b.end(), 0.0f);
    }

    /**
     * @brief ADD SAMPLES: Accumulates multi-channel input.
     */
    void addMultiSamples(const std::vector<const float*>& inputs, uint32_t len, float level) {
        for (uint32_t ch = 0; ch < std::min(static_cast<uint32_t>(inputs.size()), m_numChannels); ++ch) {
            for (uint32_t i = 0; i < len; ++i) {
                m_buffers[ch][i] += inputs[ch][i] * level;
            }
        }
    }

    const float* getChannelBuffer(uint32_t ch) const { return m_buffers[ch].data(); }

private:
    uint32_t m_id;
    std::string m_name;
    uint32_t m_numChannels;
    std::vector<std::vector<float>> m_buffers;
};

/**
 * @brief ImmersiveBusManager: Central hub for spatial routing.
 */
class ImmersiveBusManager {
public:
    static ImmersiveBusManager& getInstance() { static ImmersiveBusManager i; return i; }

    void createImmersiveBus(const std::string& name, uint32_t numChannels = 12) {
        m_buses.push_back(std::make_shared<ImmersiveBus>(static_cast<uint32_t>(m_buses.size()), name, numChannels));
    }

private:
    ImmersiveBusManager() = default;
    std::vector<std::shared_ptr<ImmersiveBus>> m_buses;
};

} // namespace Aura::Core::Engine
