#pragma once

#include <vector>
#include <array>
#include <map>
#include <string>
#include <cmath>
#include <atomic>

namespace Aura::Core::Engine {

enum class TransferCurve {
    Linear,
    Exponential,
    Logarithmic,
    SCurve
};

/**
 * @brief MacroControlManager: Clean, Lock-Free Macro Value Store.
 * HONEST REFACTOR: Simple pull-model. Macros only store their values.
 * ParamTree / ManagedParameter pull these values and apply their own mappings.
 * This completely avoids circular dependencies and is more thread-safe.
 */
class MacroControlManager {
public:
    static constexpr size_t kMaxMacros = 256;

    static MacroControlManager& getInstance() {
        static MacroControlManager instance;
        return instance;
    }

    /**
     * @brief Set the normalized [0, 1] value of a macro knob.
     * Called by UI or MIDI Controllers.
     */
    void setMacroValue(uint32_t macroIdx, float value) {
        if (macroIdx < kMaxMacros) {
            m_macroValues[macroIdx].store(value, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Get the current value. Called by ManagedParameter::getNextValue.
     */
    float getMacroValue(uint32_t macroIdx) const {
        if (macroIdx < kMaxMacros) {
            return m_macroValues[macroIdx].load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

private:
    MacroControlManager() {
        for (auto& v : m_macroValues) v.store(0.0f);
    }
    
    std::array<std::atomic<float>, kMaxMacros> m_macroValues;
};

} // namespace Aura::Core::Engine
