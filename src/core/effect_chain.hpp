#pragma once

#include <vector>
#include <memory>
#include "../dsp/iprocessor.hpp"

namespace Aura::Core {

/**
 * @brief EffectChain: Sequence of DSP processors applied to a track.
 * Manages plugin order and ensures synchronized processing.
 */
class EffectChain {
public:
    void addProcessor(std::shared_ptr<DSP::IProcessor> proc) {
        m_processors.push_back(proc);
    }

    void process(Core::AudioBuffer& buffer, Core::MidiBuffer& midi, const DSP::ProcessContext& context) {
        for (auto& proc : m_processors) {
            proc->process(buffer, midi, context);
        }
    }

    uint32_t getTotalLatencySamples() const {
        uint32_t total = 0;
        for (auto& proc : m_processors) {
            total += proc->getLatencySamples();
        }
        return total;
    }

    void setSampleRate(double sr) {
        for (auto& proc : m_processors) {
            proc->setSampleRate(sr);
        }
    }

    void clear() { m_processors.clear(); }

private:
    std::vector<std::shared_ptr<DSP::IProcessor>> m_processors;
};

} // namespace Aura::Core
