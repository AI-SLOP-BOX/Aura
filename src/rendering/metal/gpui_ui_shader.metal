#include <metal_stdlib>
using namespace metal;

/**
 * @struct GPUI_Quad
 * @brief Zero-Allocation GPU Geometry for a single UI element (Button, Panel, Track).
 * Consistent with Zed's GPUI architecture: 1 Instance = 1 Quad.
 */
struct GPUI_Quad {
    float4 rect;  // [x, y, w, h]
    float4 color; // [r, g, b, a]
    float4 borderRadius; // [top-left, top-right, bottom-right, bottom-left]
    float4 border; // [width, r, g, b]
    float4 shadow; // [offset_x, offset_y, blur, opacity]
};

struct VertexOut {
    float4 position [[position]];
    float2 uv;
    float4 color;
    float4 rect;
    float4 borderRadius;
};

/**
 * @vertex GPUI_Vertex
 * @brief High-speed Instanced Vertex Program.
 */
vertex VertexOut gpui_vertex(uint vertexID [[vertex_id]],
                               uint instanceID [[instance_id]],
                               constant GPUI_Quad* quads [[buffer(0)]],
                               constant float2& viewSize [[buffer(1)]]) {
    GPUI_Quad q = quads[instanceID];
    VertexOut out;
    
    // Quad vertices [0,0] to [1,1]
    float2 pos[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
    float2 localPos = pos[vertexID];
    
    float2 worldPos = q.rect.xy + localPos * q.rect.zw;
    
    // Normalize to Metal Clip Space (-1, 1)
    out.position = float4((worldPos / viewSize * 2.0) - 1.0, 0.0, 1.0);
    out.position.y *= -1; // Metal Y is down
    
    out.uv = localPos * q.rect.zw; // Pixels
    out.color = q.color;
    out.rect = q.rect;
    out.borderRadius = q.borderRadius;
    
    return out;
}

/**
 * @fragment GPUI_Fragment
 * @brief SDF-Based Rectangle Shader for pixel-perfect curves and shadows.
 * This is how Zed achieves its legendary 'Snappy' and 'Smooth' UI.
 */
float sdf_rect(float2 p, float2 size, float4 radius) {
    float r = (p.x > 0) ? ((p.y > 0) ? radius[2] : radius[1]) : ((p.y > 0) ? radius[3] : radius[0]);
    float2 d = abs(p) - size + r;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

fragment float4 gpui_fragment(VertexOut in [[stage_in]]) {
    float2 center = in.rect.zw / 2.0;
    float2 p = in.uv - center;
    
    float d = sdf_rect(p, center, in.borderRadius);
    
    // Anti-aliased stroke and fill
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    if (alpha <= 0.0) discard_fragment();
    
    return float4(in.color.rgb, in.color.a * alpha);
}
