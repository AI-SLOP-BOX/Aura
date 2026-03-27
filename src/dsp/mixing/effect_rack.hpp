#pragma once

#include <vector>
#include <memory>
#include <string>

namespace Aura::Core::DSP::Mixing {

/**
 * @brief IAudioEffect: Abstract interface for any processable DSP block.
 */
class IAudioEffect {
public:
    virtual ~IAudioEffect() = default;
    virtual void process(float* l, float* r, size_t numFrames) = 0;
    virtual void setBypass(bool bp) = 0;
};

/**
 * @brief EffectRack: Professional Serial Insert chain.
 * Logic Pro-style "Insert Slots" where you can stack EQ, Delay, and Reverb.
 */
class EffectRack {
public:
    void addEffect(std::unique_ptr<IAudioEffect> fx) {
        m_effects.push_back(std::move(fx));
    }

    /**
     * @brief Processes the audio through all active effects in the rack.
     */
    void process(float* l, float* r, size_t numFrames) {
        for (auto& fx : m_effects) {
            fx->process(l, r, numFrames);
        }
    }

    void clear() { m_effects.clear(); }

private:
    std::vector<std::unique_ptr<IAudioEffect>> m_effects;
};

} // namespace Aura::Core::DSP::Mixing
