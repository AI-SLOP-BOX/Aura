#include "audio_interface.hpp"

namespace Aura::IO {

/**
 * @brief CoreAudioDriver: Platform-specific implementation for macOS.
 * Addresses "driver implementation separation" and "fat headers" from the review.
 */
class CoreAudioDriver : public IAudioInterface {
public:
    bool initialize(double sr, uint32_t bs) override {
        // CoreAudio HAL (AudioObjectSetProperty etc.) logic would be here
        return true;
    }

    void start() override {
        // AudioOutputUnitStart logic
    }

    void stop() override {
        // AudioOutputUnitStop logic
    }

    std::string getDeviceName() const override {
        return "macOS CoreAudio Engine";
    }
};

/**
 * @brief HardwareFactory: Implementation updated to return platform drivers.
 */
std::unique_ptr<IAudioInterface> HardwareFactory::createDefault() {
    return std::make_unique<CoreAudioDriver>();
}

} // namespace Aura::IO
