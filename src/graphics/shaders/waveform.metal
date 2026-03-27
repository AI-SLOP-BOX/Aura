#include <metal_stdlib>
using namespace metal;

/**
 * @brief WaveformShader: High-speed GPU-accelerated audio visualization.
 * Renders millions of samples using a vertex shader to minimize draw calls.
 */

struct VertexOut {
    uint vertexID [[vertex_id]];
    float4 position [[position]];
    float4 color;
};

struct WaveformData {
    float2 minMax; // [min_peak, max_peak]
};

vertex VertexOut waveform_vertex(
    uint vid [[vertex_id]],
    constant WaveformData *data [[buffer(0)]],
    constant float2 &view_range [[buffer(1)]], // [startX, endX]
    constant float4 &draw_color [[buffer(2)]]
) {
    VertexOut out;
    
    // 1. Position mapping: X is time, Y is amplitude
    float x = (float)vid / (view_range.y - view_range.x);
    float y = (vid % 2 == 0) ? data[vid/2].minMax.x : data[vid/2].minMax.y;
    
    out.position = float4(x * 2.0 - 1.0, y, 0.0, 1.0);
    out.color = draw_color;
    
    return out;
}

fragment float4 waveform_fragment(VertexOut in [[stage_in]]) {
    return in.color;
}
