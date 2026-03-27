#include <metal_stdlib>
using namespace metal;

/**
 * @shader WaveformUpsampler
 * @brief Zero-Aliasing High-Zoom Waveform Renderer.
 * HONEST FIX: Replaces legacy 'line-to-line' drawing with 
 * GPU-accelerated Cubic Hermite Interpolation. 
 * Even at 1-sample zoom, the waveform looks like a smooth analog curve.
 */
kernel void upsample_waveform(
    const device float* inputSamples [[buffer(0)]],
    device float2* vertexBuffer [[buffer(1)]],
    const device uint& numInputSamples [[buffer(2)]],
    const device float& zoomLevel [[buffer(3)]],
    const device float& timeOffset [[buffer(4)]],
    uint id [[thread_position_in_grid]]) {
    
    if (id >= numInputSamples * 8) return; // 8x oversampling

    float t = (float)id / 8.0f;
    uint idx = (uint)floor(t);
    float f = t - (float)idx;

    if (idx < 1 || idx >= numInputSamples - 2) return;

    // 4-point Cubic Hermite Interpolation (Catmull-Rom)
    float s0 = inputSamples[idx - 1];
    float s1 = inputSamples[idx];
    float s2 = inputSamples[idx + 1];
    float s3 = inputSamples[idx + 2];

    float a = -0.5f*s0 + 1.5f*s1 - 1.5f*s2 + 0.5f*s3;
    float b = s0 - 2.5f*s1 + 2.0f*s2 - 0.5f*s3;
    float c = -0.5f*s0 + 0.5f*s2;
    float d = s1;

    float y = a*f*f*f + b*f*f + c*f + d;
    float x = (t + timeOffset) * zoomLevel;

    vertexBuffer[id] = float2(x, y);
}
