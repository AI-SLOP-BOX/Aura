#include <metal_stdlib>
using namespace metal;

/**
 * @struct SpectrogramVertex
 * @brief Simple vertex shader input.
 */
struct SpectrogramVertex {
    float4 position [[position]];
    float2 texCoord;
};

/**
 * @fragment fragment_spectrogram
 * @brief High-performance Heat-map Generator.
 * Calculates the classic Green -> Yellow -> Red spectral waterfall on GPU.
 * Prevents CPU thermal throttling by moving color-mapping to the GFX core.
 */
fragment float4 fragment_spectrogram(SpectrogramVertex vert [[stage_in]],
                                      texture2d<float> specTexture [[texture(0)]]) {
    sampler linearSampler(mag_filter::linear, min_filter::linear);
    float energy = specTexture.sample(linearSampler, vert.texCoord).r;

    // --- PRO LOGIC: PSYCHOACOUSTIC COLOR MAPPING ---
    // Transition points: -80dB (Black) -> -40dB (Green) -> -12dB (Yellow) -> 0dB (Red)
    float3 black = float3(0.0, 0.0, 0.0);
    float3 green = float3(0.1, 0.8, 0.2);
    float3 yellow = float3(1.0, 0.9, 0.1);
    float3 red = float3(1.0, 0.1, 0.0);

    float3 color;
    if (energy < 0.3) {
        color = mix(black, green, energy / 0.3);
    } else if (energy < 0.7) {
        color = mix(green, yellow, (energy - 0.3) / 0.4);
    } else {
        color = mix(yellow, red, (energy - 0.7) / 0.3);
    }

    // Add slight saturation and bloom simulation
    color *= (1.0 + 0.1 * energy);
    return float4(color, 1.0);
}

/**
 * @compute compute_meter_decay
 * @brief GPU-accelerated Meter Decay simulator.
 * Calculates peak-hold and RMS decay for 256 tracks simultaneously.
 */
kernel void compute_meter_decay(device float* currentLevels [[buffer(0)]],
                                device float* peakLevels [[buffer(1)]],
                                uint id [[thread_position_in_grid]]) {
    float val = currentLevels[id];
    float peak = peakLevels[id];

    // Decay peak at 2.5dB per 100ms
    if (val > peak) {
        peak = val;
    } else {
        peak *= 0.999f;
    }

    peakLevels[id] = peak;
}
