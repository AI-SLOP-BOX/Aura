#pragma once

#include "audio_driver_base.hpp"
#include <memory>
#include <string>

namespace Aura::IO::Drivers {

/**
 * @brief DriverFactory: Platform-aware factory for audio drivers.
 * Provides a Null-ptr safe mechanism to retrieve the best available audio API.
 */
class DriverFactory {
public:
    enum class API { Auto, ASIO, CoreAudio, WASAPI, PipeWire, Dummy };

    /**
     * @brief Creates the best driver for the current system.
     * @param api: Preferred API. Defaults to platform-native.
     * @return A valid IDriver instance. If requested API is unavailable, returns a Silent/Dummy driver.
     */
    static std::unique_ptr<IDriver> create(API api = API::Auto);

    /**
     * @brief SilentDriver: A safe fallback that produces no sound but prevents engine crashes.
     */
    class SilentDriver : public IDriver {
    public:
        bool initialize(const Config& config) override { m_config = config; return true; }
        bool start(ProcessCallback callback) override { return true; }
        void stop() override {}
        std::string getDriverName() const override { return "Aura Silent Audio Engine"; }
        double getSampleRate() const override { return m_config.sampleRate; }
        uint32_t getBufferSize() const override { return m_config.bufferSize; }
    private:
        Config m_config;
    };
};

} // namespace Aura::IO::Drivers
