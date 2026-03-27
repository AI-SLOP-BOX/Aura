#pragma once
#include <vector>
#include <memory>
#include "../dsp/iprocessor.hpp"
#include <atomic>

namespace Aura::Core::Engine {

/**
 * @class EffectChain
 * @brief Thread-Safe Serial Processor Chain for Tracks.
 * HONEST FIX: Uses atomic shared_ptr to swap chains without audio glitching.
 */
class EffectChain {
public:
    EffectChain() {
        m_processors.store(std::make_shared<std::vector<std::shared_ptr<DSP::IProcessor>>>());
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const DSP::ProcessContext& context) {
        auto current = m_processors.load(std::memory_order_acquire);
        if (!current) return;

        for (auto& p : *current) {
            if (p) p->process(buffer, midi, context);
        }
    }

    /**
     * @brief INSERTION: RT-Safe plugin injection.
     */
    void addProcessor(std::shared_ptr<DSP::IProcessor> p) {
        auto oldList = m_processors.load(std::memory_order_acquire);
        while (true) {
            auto newList = std::make_shared<std::vector<std::shared_ptr<DSP::IProcessor>>>(*oldList);
            newList->push_back(p);
            if (m_processors.compare_exchange_weak(oldList, newList)) break;
        }
    }

    void clear() {
        m_processors.store(std::make_shared<std::vector<std::shared_ptr<DSP::IProcessor>>>());
    }

private:
    std::atomic<std::shared_ptr<std::vector<std::shared_ptr<DSP::IProcessor>>>> m_processors;
};

} // namespace Aura::Core::Engine
