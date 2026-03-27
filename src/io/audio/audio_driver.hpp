#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include <chrono>
#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#endif

namespace Aura::IO::Audio {

/**
 * @class RealtimeAudioDriver
 * @brief High-Priority Real-time Audio Engine Driver.
 */
class RealtimeAudioDriver {
public:
    void start(::Aura::AuraEngine& engine, double sr, uint32_t bs) {
        m_running = true;
        m_audioThread = std::thread([this, &engine, sr, bs]() {
            #ifdef __APPLE__
            thread_time_constraint_policy_data_t policy;
            policy.period = (uint32_t)(1e9 * bs / sr);
            policy.computation = (uint32_t)(policy.period * 0.85);
            policy.constraint = policy.computation;
            policy.preemptible = 1;
            thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY, 
                             (thread_policy_t)&policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
            #endif

            std::vector<float> L(bs), R(bs);
            auto next = std::chrono::steady_clock::now();
            auto dur = std::chrono::nanoseconds((long)(1e9 * bs / sr));

            while (m_running) {
                engine.process(L.data(), R.data(), bs);
                next += dur;
                std::this_thread::sleep_until(next);
            }
        });
    }

    void stop() {
        m_running = false;
        if (m_audioThread.joinable()) m_audioThread.join();
    }

private:
    std::atomic<bool> m_running{false};
    std::thread m_audioThread;
};

} // namespace Aura::IO::Audio
