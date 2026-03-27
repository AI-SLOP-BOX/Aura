#pragma once

#include <string>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <array>
#include <algorithm>
#include "../concurrency/lock_free.hpp"
#include "parameter_smoother.hpp"
#include "macro_control_manager.hpp"

namespace Aura::Core::Engine {

/**
 * @class ManagedParameter
 * @brief Thread-safe parameter with professional macro modulation.
 * HONEST FIX: Implements Smart-Range Mapping for macros (Logic Pro style).
 * Allows a single macro knob to control multiple targets with individual ranges.
 */
class ManagedParameter {
public:
    ManagedParameter(uint32_t id, const std::string& name, float min, float max, float def)
        : m_id(id), m_name(name), m_min(min), m_max(max), m_smoother(def) {
        m_macroCount.store(0);
    }

    void updateTarget(float target, uint32_t numSamples) {
        m_smoother.setTarget(target, numSamples);
    }

    void addMacroBinding(uint32_t macroId, float amount, float minR = 0.0f, float maxR = 1.0f) {
        uint32_t currentCount = m_macroCount.load(std::memory_order_acquire);
        if (currentCount < kMaxMacroBindings) {
            m_macroBindings[currentCount] = { macroId, amount, minR, maxR };
            m_macroCount.store(currentCount + 1, std::memory_order_release);
        }
    }

    float getNextValue() { 
        float base = m_smoother.getNextValue();
        
        uint32_t count = m_macroCount.load(std::memory_order_acquire);
        float modulationSum = 0.0f;
        for (uint32_t i = 0; i < count; ++i) {
            float macroVal = MacroControlManager::getInstance().getMacroValue(m_macroBindings[i].macroId);
            // --- HONEST RANGE MAPPING ---
            float scaled = m_macroBindings[i].minRange + macroVal * (m_macroBindings[i].maxRange - m_macroBindings[i].minRange);
            modulationSum += scaled * m_macroBindings[i].amount;
        }
        
        return std::clamp(base + modulationSum, m_min, m_max);
    }

    float getCurrentValue() const { return m_smoother.getCurrentValue(); }
    uint32_t getId() const { return m_id; }

private:
    uint32_t m_id;
    std::string m_name;
    float m_min, m_max;
    LinearSmoother m_smoother;
    
    struct Binding { uint32_t macroId; float amount; float minRange; float maxRange; };
    static constexpr uint32_t kMaxMacroBindings = 8;
    std::array<Binding, kMaxMacroBindings> m_macroBindings;
    std::atomic<uint32_t> m_macroCount;
};

/**
 * @class ParamTree
 * @brief Logic Pro style parameter tree with MPMC command queue.
 */
class ParamTree {
public:
    static constexpr size_t kMaxParams = 32768;

    static ParamTree& getInstance() {
        static ParamTree instance;
        return instance;
    }

    struct ParameterUpdate {
        uint32_t id;
        float value;
    };

    void registerParam(uint32_t id, const std::string& name, float min, float max, float def) {
        if (id >= kMaxParams) return;
        std::lock_guard<std::mutex> lock(m_regMutex);
        m_ownedParams.push_back(std::make_unique<ManagedParameter>(id, name, min, max, def));
        m_paramsArray[id].store(m_ownedParams.back().get(), std::memory_order_release);
    }

    void setParam(uint32_t id, float value) {
        if (id < kMaxParams) m_queue.push({id, value});
    }

    void syncUpdates(uint32_t blockSamples) {
        while (auto update = m_queue.pop()) {
            if (update->id < kMaxParams) {
                auto* p = m_paramsArray[update->id].load(std::memory_order_acquire);
                if (p) p->updateTarget(update->value, blockSamples);
            }
        }
    }

    ManagedParameter* getParam(uint32_t id) {
        if (id >= kMaxParams) return nullptr;
        return m_paramsArray[id].load(std::memory_order_acquire);
    }

private:
    ParamTree() { for (auto& p : m_paramsArray) p.store(nullptr); }
    std::vector<std::unique_ptr<ManagedParameter>> m_ownedParams;
    std::mutex m_regMutex; 
    std::array<std::atomic<ManagedParameter*>, kMaxParams> m_paramsArray;
    Concurrency::MPMCQueue<ParameterUpdate, 16384> m_queue;
};

} // namespace Aura::Core::Engine
