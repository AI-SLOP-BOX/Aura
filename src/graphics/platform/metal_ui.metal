#include <metal_stdlib>
using namespace metal;

struct Uniforms {
    float4 pos;
    float4 props;
    float4 color1;
    float4 color2;
};

struct VertexOut {
    float4 position [[position]];
    float2 uv;
    float4 col1;
    float4 col2;
    float4 p;
};

float sdRoundRect(float2 p, float2 b, float r) {
    float2 d = abs(p) - b + r;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

float sdGlyph(float2 p, int c) {
    if (c == 83) { // 'S'
        float d1 = sdRoundRect(p + float2(0.0,-0.6), float2(0.4,0.1), 0.01);
        float d2 = sdRoundRect(p + float2(0.0,0.6), float2(0.4,0.1), 0.01);
        float d3 = sdRoundRect(p, float2(0.4,0.1), 0.01);
        float d4 = sdRoundRect(p + float2(-0.3,-0.3), float2(0.1,0.3), 0.01);
        float d5 = sdRoundRect(p + float2(0.3,0.3), float2(0.1,0.3), 0.01);
        return min(min(min(d1, d2), d3), min(d4, d5));
    }
    if (c == 77) { // 'M'
        float d1 = sdRoundRect(p + float2(-0.4,0.0), float2(0.1,0.6), 0.01);
        float d2 = sdRoundRect(p + float2(0.4,0.0), float2(0.1,0.6), 0.01);
        float d3 = sdRoundRect(p + float2(-0.2,-0.2), float2(0.1,0.4), 0.2);
        float d4 = sdRoundRect(p + float2(0.2,-0.2), float2(0.1,0.4), 0.2);
        return min(min(d1, d2), min(d3, d4));
    }
    if (c == 65) { // 'A'
        float d1 = sdRoundRect(p + float2(-0.4,0.1), float2(0.1,0.5), 0.01);
        float d2 = sdRoundRect(p + float2(0.4,0.1), float2(0.1,0.5), 0.01);
        float d3 = sdRoundRect(p + float2(0.0,-0.4), float2(0.4,0.1), 0.01);
        float d4 = sdRoundRect(p + float2(0.0,0.1), float2(0.3,0.1), 0.01);
        return min(min(d1, d2), min(d3, d4));
    }
    if (c == 73) { // 'I'
        float d1 = sdRoundRect(p, float2(0.1, 0.6), 0.01);
        float d2 = sdRoundRect(p + float2(0.0, 0.6), float2(0.3, 0.1), 0.01);
        float d3 = sdRoundRect(p + float2(0.0, -0.6), float2(0.3, 0.1), 0.01);
        return min(min(d1, d2), d3);
    }
    if (c == 45) { // '-'
        return sdRoundRect(p, float2(0.4, 0.1), 0.01);
    }
    if (c >= 48 && c <= 57) { // 0-9 (Approximation as squares/rects for now)
        return sdRoundRect(p, float2(0.3, 0.5), 0.1);
    }
    if (c == 8) return length(p) - 0.7; // Record
    return sdRoundRect(p, float2(0.2, 0.2), 0.05); // Generic glyph
}

vertex VertexOut vUI(uint vid [[vertex_id]], uint instance [[instance_id]], constant Uniforms* u [[buffer(0)]]) {
    VertexOut out;
    float2 quad[4] = { float2(0,0), float2(1,0), float2(0,1), float2(1,1) };
    float2 uv = quad[vid];
    Uniforms cur = u[instance];
    float2 pos = (cur.pos.xy + uv * cur.pos.zw) - 1.0;
    out.position = float4(pos.x, -pos.y, 0.0, 1.0);
    out.uv = uv;
    out.col1 = cur.color1;
    out.col2 = cur.color2;
    out.p = cur.props;
    return out;
}

fragment float4 fUI(VertexOut in [[stage_in]]) {
    float type = in.p.w;
    float2 p = (in.uv - 0.5) * 2.0;

    if (type < 0.5) {
        float d = sdRoundRect(p, float2(1.0 - in.p.x * 0.05), in.p.x);
        float alpha = 1.0 - smoothstep(-0.02, 0.02, d);
        float4 base = mix(in.col1, in.col2, in.uv.y);
        float bevel = 1.0 - smoothstep(0.0, 0.2, d + 0.1);
        base.rgb += bevel * 0.1;
        return base * alpha;
    }
    
    if (type > 2.5 && type < 3.5) {
        float d = sdRoundRect(p, float2(0.98), in.p.x);
        float alpha = 1.0 - smoothstep(-0.01, 0.01, d);
        return mix(float4(1.0, 1.0, 1.0, 0.15), float4(1.0, 1.0, 1.0, 0.05), in.uv.y) * alpha;
    }

    if (type > 3.5) {
        float d = sdGlyph(p, (int)in.p.x);
        float alpha = 1.0 - smoothstep(-0.15, 0.15, d); // Softer font
        return in.col1 * alpha;
    }

    return in.col1;
}
