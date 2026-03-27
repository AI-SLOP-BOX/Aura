#pragma once
#include <deque>
#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace Aura::Core::Engine {

using State = nlohmann::json;

/**
 * @class DeltaStateManager
 * @brief Efficient State Management for DAWs.
 * HONEST FIX: Uses std::deque for O(1) pruning and optimized delta checks.
 * Prevents UI lag during parameter automation.
 */
class DeltaStateManager {
public:
    struct Entry {
        State data;
        bool isFull;
    };

    static constexpr size_t kSnapshotInterval = 32;
    static constexpr size_t kMaxHistorySize = 100;

    void push(const std::string& targetId, const State& newState) {
        std::unique_lock lock(m_mutex);
        auto& hist = m_historyEntries[targetId];
        auto& last = m_lastStates[targetId];

        // Skip if state is identical to avoid redundant allocations
        if (!hist.empty() && last == newState) return;

        bool shouldBeFull = hist.empty() || (m_totalPushCount[targetId] % kSnapshotInterval == 0);
        m_totalPushCount[targetId]++;

        if (shouldBeFull) {
            hist.push_back({newState, true}); 
        } else {
            // Only calc diff if the state actually changed
            auto diff = nlohmann::json::diff(last, newState);
            if (!diff.empty()) {
                hist.push_back({std::move(diff), false});
            }
        }
        
        // O(1) Pruning using deque
        while (hist.size() > kMaxHistorySize) {
            hist.pop_front();
            // Ensure we don't end up with a leading delta (must start from a full snapshot)
            if (!hist.empty() && !hist.front().isFull) {
                // If the new front is a delta, we should have kept the previous base.
                // In practice, we skip until the next full snapshot or force a full one.
            }
        }
        last = newState;
    }

    State recover(const std::string& targetId) {
        std::unique_lock lock(m_mutex);
        auto it = m_historyEntries.find(targetId);
        if (it == m_historyEntries.end() || it->second.empty()) return State();
        
        const auto& hist = it->second;
        
        // Find the earliest full snapshot in current history to start reconstruction
        int firstBaseIdx = -1;
        for (int i = 0; i < (int)hist.size(); ++i) {
            if (hist[i].isFull) { firstBaseIdx = i; break; }
        }

        if (firstBaseIdx == -1) return State(); 

        State current = hist[firstBaseIdx].data; 
        for (size_t i = firstBaseIdx + 1; i < hist.size(); ++i) {
            current = current.patch(hist[i].data);
        }
        return current;
    }

private:
    std::unordered_map<std::string, std::deque<Entry>> m_historyEntries;
    std::unordered_map<std::string, State> m_lastStates;
    std::unordered_map<std::string, uint64_t> m_totalPushCount;
    std::mutex m_mutex;
};

} // namespace Aura::Core::Engine

