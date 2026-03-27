#pragma once

#include <vector>
#include <string>

namespace Aura::Core {

/**
 * @brief SampleBuffer: Zero-copy reference to audio data.
 * Used for massive library handling without memory overhead.
 */
struct SampleBuffer {
    const float* data = nullptr;
    size_t length = 0;
};

/**
 * @brief SamplerZone: Mapping information for MIDI-to-Sample routing.
 */
struct SamplerZone {
    uint8_t rootKey = 60;
    uint8_t lowKey = 0;
    uint8_t highKey = 127;
    SampleBuffer buffer;
};

} // namespace Aura::Core
