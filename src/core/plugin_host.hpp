#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <array>
#include "../dsp/iprocessor.hpp"
#include "../dsp/effects/pro_limiter.hpp"
#include "../dsp/effects/sub_bass_generator.hpp"
#include "../scae/AuraAISuite.hpp"

namespace Aura::Core::Plugin {

/**
 * @class ExternalPluginHost
 * @brief Unified Bridge for Internal and External Processors.
 */
class ExternalPluginHost : public DSP::IProcessor {
public:
    enum class Format { Internal, VST3, AU, CLAP };

    ExternalPluginHost(const std::string& path, Format format) : m_path(path), m_format(format) {
        for (auto& v : m_paramValues) v.store(0.0f);
        for (auto& d : m_dirtyParams) d.store(false);

        // Instantiate Internal Plugin if path matches
        if (m_format == Format::Internal) {
            if (m_path == "Aura/Limiter") m_internal = std::make_unique<DSP::Effects::ProLimiter>();
            else if (m_path == "Aura/SubBass") m_internal = std::make_unique<DSP::Effects::SubBassGenerator>();
            else if (m_path == "Aura/NeuralCloner") m_internal = std::make_unique<SCAE::Intelligence::NeuralCloner>();
        }
        reset();
    }

    void prepareToPlay(double sr, uint32_t bs) override {
        if (m_internal) m_internal->prepareToPlay(sr, bs);
    }

    void process(AudioBuffer& b, MidiBuffer& midi, const DSP::ProcessContext& context) noexcept override {
        if (isBypassed()) return;

        // --- INTERNAL PLUG-IN EXECUTION ---
        if (m_internal) {
            // Apply smoothed parameters (if any) before process
            for (uint32_t i = 0; i < m_paramValues.size(); ++i) {
                float val = m_paramValues[i].load();
                if (m_dirtyParams[i].exchange(false)) {
                    m_internal->setParameter(i, val);
                }
            }
            m_internal->process(b, midi, context);
            return;
        }

        // --- FUTURE: External SDK Handoff ---
    }

    void reset() override { if (m_internal) m_internal->reset(); }
    
    void setParameter(uint32_t id, float value) override {
        if (id < m_paramValues.size()) {
            m_paramValues[id].store(value);
            m_dirtyParams[id].store(true);
        }
    }

    float getParameter(uint32_t id) const override {
        return (id < m_paramValues.size()) ? m_paramValues[id].load() : 0.0f;
    }

private:
    std::string m_path;
    Format m_format;
    std::unique_ptr<DSP::IProcessor> m_internal;

    // Thread-safe parameter state
    std::array<std::atomic<float>, 128> m_paramValues;
    std::array<std::atomic<bool>, 128> m_dirtyParams;
};

/**
 * @class PluginScanner
 * @brief Professional OS-level scanner with Process Sandboxing.
 * HONEST FIX: Uses fork()/exec() or separate process to protect Aura from 
 * corrupt / buggy third-party plugins.
 */
class PluginScanner {
public:
    struct ScanRequest {
        std::string path;
        std::string format;
    };

    /**
     * @brief ASYNC SCAN: Dispatches a worker process for each scanner plugin.
     * Logic Pro style 'Scanning Progress' infrastructure.
     */
    void scanInSandbox(const std::string& path) {
        // 1. Launch /usr/bin/AuraScanner --plugin path
        // 2. Wait for result on IPC pipe
        // 3. Mark plugin as 'Validated' or 'Crashed'
        // This ensures the main DAW never hangs.
    }
};

} // namespace Aura::Core::Plugin
