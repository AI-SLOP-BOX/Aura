#pragma once

#include "audio_drivers.hpp"

namespace Aura::IO {
    // Deprecated: Use IAudioDriver 
    using IAudioInterface = IAudioDriver;
    using DummyHardware = DummyAudioDriver;

    class HardwareFactory {
    public:
        static std::unique_ptr<IAudioInterface> createDefault();
    };
} // namespace Aura::IO
