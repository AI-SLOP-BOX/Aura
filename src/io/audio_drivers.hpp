#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace Aura::IO {

/**
 * @brief IAudioDriver: Physical hardware communication layer.
 */
class IAudioDriver {
public:
    using AudioCallback = std::function<void(float** /*out*/, float** /*in*/, uint32_t /*numFrames*/)>;

    virtual ~IAudioDriver() = default;
    virtual bool initialize(double sampleRate, uint32_t bufferSize) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::string getDeviceName() const = 0;
    
    void setCallback(AudioCallback cb) { m_callback = std::move(cb); }

protected:
    AudioCallback m_callback;
};

/**
 * @brief DummyAudioDriver: Safe silent fallback for hardware-less environments.
 */
class DummyAudioDriver : public IAudioDriver {
public:
    bool initialize(double sr, uint32_t bs) override { return true; }
    void start() override {}
    void stop() override {}
    std::string getDeviceName() const override { return "Aura Silent Engine"; }
};

/**
 * @brief DriverFactory: Singleton factory for platform-specific hardware drivers.
 */
class DriverFactory {
public:
    static std::unique_ptr<IAudioDriver> createDefault() {
#ifdef __APPLE__
        // Placeholder for CoreAudioDriver implementation
        return std::make_unique<DummyAudioDriver>();
#elif defined(_WIN32)
        // Placeholder for ASIODriver
        return std::make_unique<DummyAudioDriver>();
#else
        return std::make_unique<DummyAudioDriver>();
#endif
    }
};

} // namespace Aura::IO
