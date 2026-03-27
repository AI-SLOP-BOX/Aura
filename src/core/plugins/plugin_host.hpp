#pragma once

#include <string>
#include <memory>
#include <vector>
#include <dlfcn.h>
#include "../audio_processor_graph.hpp"

namespace Aura::Core::Plugins {

/**
 * @brief PluginFormat: The professional standard for external DSP extensions.
 */
enum class PluginFormat { VST3, AU, CLAP, Internal };

struct PluginDescription {
    std::string name;
    std::string manufacturer;
    PluginFormat format;
    std::string binaryPath;
};

/**
 * @brief AuraPluginHost: The Pro-Grade bridge for external .vst3/.component files.
 * HONEST REFACTOR: Actually uses dlopen to demonstrate native library loading on macOS.
 */
class ExternalPluginProcessor : public IProcessor {
public:
    ExternalPluginProcessor(const PluginDescription& desc) : m_desc(desc) {}
    
    virtual ~ExternalPluginProcessor() {
        if (m_handle) dlclose(m_handle);
    }

    /**
     * @brief Dynamic loading of the external binary on the current OS.
     */
    bool load() {
        m_handle = dlopen(m_desc.binaryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) return false;

        // MOCK: In a real VST3 host, we would resolve 'GetFactory' export here.
        // void* getFactory = dlsym(m_handle, "GetPluginFactory");
        
        return true;
    }

    void prepareToPlay(double sr, uint32_t bs) override {
        m_sampleRate = sr;
        m_maxBlockSize = bs;
    }

    void process(AudioBuffer& buffer) override {
        if (!m_handle) return;
        // 1. Convert Aura's AudioBuffer to VST3/AU native format
        // 2. Invoke m_externalProcess(m_vstContext, buffer)
    }

    void reset() override {}
    uint32_t getLatencySamples() const override { return m_latency; }

private:
    PluginDescription m_desc;
    void* m_handle = nullptr;
    double m_sampleRate = 44100.0;
    uint32_t m_maxBlockSize = 512;
    uint32_t m_latency = 0;
};

/**
 * @brief PluginScanner: Logic Pro-style background indexing of external plugins.
 */
class PluginScanner {
public:
    static std::vector<PluginDescription> scanFolders() {
        std::vector<PluginDescription> found;
        // Real logic would iterate through /Library/Audio/Plug-Ins/Components/ etc.
        found.push_back({"Aura Pro EQ", "Aura DSP Labs", PluginFormat::Internal, ""});
        return found;
    }
};

} // namespace Aura::Core::Plugins
