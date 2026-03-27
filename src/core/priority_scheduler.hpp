#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace Aura::Core {

/**
 * @brief PriorityScheduler: A unified thread pool for DAW-scaled parallel tasks.
 * Addresses "fragmented threading" and "std::async bottlenecks" from the review.
 */
class PriorityScheduler {
public:
    enum class Priority { Audio = 0, IO = 1, Analyze = 2, UI = 3 };

    PriorityScheduler(size_t threads) : m_stop(false) {
        for(size_t i=0; i<threads; ++i) {
            m_workers.emplace_back([this]{
                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_queueMutex);
                        m_condition.wait(lock, [this]{
                            return m_stop || !m_queues[0].empty() || !m_queues[1].empty() || 
                                   !m_queues[2].empty() || !m_queues[3].empty();
                        });
                        if(m_stop) return;

                        // Process from highest to lowest priority
                        for(auto& q : m_queues) {
                            if(!q.empty()) {
                                task = std::move(q.front());
                                q.pop();
                                break;
                            }
                        }
                    }
                    if(task) task();
                }
            });
        }
    }

    ~PriorityScheduler() {
        { std::unique_lock<std::mutex> lock(m_queueMutex); m_stop = true; }
        m_condition.notify_all();
        for(auto& w : m_workers) w.join();
    }

    void enqueue(Priority p, std::function<void()> task) {
        { std::unique_lock<std::mutex> lock(m_queueMutex); m_queues[static_cast<int>(p)].push(std::move(task)); }
        m_condition.notify_one();
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_queues[4];
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop;
};

} // namespace Aura::Core
