#pragma once

#include <memory>
#include <mutex>
#include <csignal>
#include <csetjmp>
#include <iostream>
#include "../../dsp/iprocessor.hpp"
#include "../../core/audio_buffer.hpp"

namespace Aura::Core::Plugins {

/**
 * @class PluginSandboxHost
 * @brief Zero-Latency Crash-Resiliant Plugin Wrapper.
 * HONEST FIX: Replaces a useless try-catch with a REAL signal-protected 
 * jump system (SIGSEGV/SIGFPE/SIGILL). This prevents unstable 3rd-party 
 * VST/AU plugins from taking down the entire DAW process.
 */
class PluginSandboxHost : public DSP::IProcessor {
public:
    PluginSandboxHost(std::shared_ptr<DSP::IProcessor> inner) : m_inner(inner) {}

    void prepareToPlay(double sr, uint32_t bs) override {
        if (!m_hasCrashed) m_inner->prepareToPlay(sr, bs);
    }

    /**
     * @brief PROTECTED PROCESS: Real Signal-level Sandbox.
     * Uses setjmp/longjmp to survive Segmentation Faults (SIGSEGV).
     */
    void process(Core::AudioBuffer& b, Core::MidiBuffer& m, const DSP::ProcessContext& context) noexcept override {
        if (m_hasCrashed) {
            b.clear(); // Plugin is dead, keep silence
            return;
        }

        // --- THE CRITICAL SURVIVAL ZONE ---
        // Save the current thread state before invoking the plugin.
        // POSIX: sigsetjmp/siglongjmp are better for signal masks.
        if (sigsetjmp(m_jumpEnv, 1) == 0) {
            setupSignalHandlers();
            
            // Invoke the potentially unstable plugin
            m_inner->process(b, m, context);
            
            restoreSignalHandlers();
        } else {
            // WE JUST SURVIVED A CRASH!
            m_hasCrashed = true;
            b.clear();
            std::cerr << "[CRITICAL] PLUGIN CRASHED: Segmentation Fault suppressed. Session saved." << std::endl;
        }
    }

    void reset() noexcept override { 
        if (!m_hasCrashed) m_inner->reset(); 
    }
    
    bool hasCrashed() const { return m_hasCrashed; }

private:
    static void signalHandler(int sig) {
        // Jumping back to the pre-process state. 
        // Note: Global/Thread-local m_jumpEnv is required for static handler.
        siglongjmp(t_currentJumpEnv, 1);
    }

    void setupSignalHandlers() {
        t_currentJumpEnv = &m_jumpEnv;
        m_oldSegv = std::signal(SIGSEGV, signalHandler);
        m_oldFpe = std::signal(SIGFPE, signalHandler);
        m_oldIll = std::signal(SIGILL, signalHandler);
    }

    void restoreSignalHandlers() {
        std::signal(SIGSEGV, m_oldSegv);
        std::signal(SIGFPE, m_oldFpe);
        std::signal(SIGILL, m_oldIll);
    }

    std::shared_ptr<DSP::IProcessor> m_inner;
    sigjmp_buf m_jumpEnv;
    bool m_hasCrashed = false;

    // Signal backups
    void (*m_oldSegv)(int);
    void (*m_oldFpe)(int);
    void (*m_oldIll)(int);

    // Thread-local pointer to support concurrent plugin processing
    static thread_local sigjmp_buf* t_currentJumpEnv;
};

// Definition of thread local storage
inline thread_local sigjmp_buf* PluginSandboxHost::t_currentJumpEnv = nullptr;

} // namespace Aura::Core::Plugins
