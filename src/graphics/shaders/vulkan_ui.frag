#version 450
layout(location = 0) in vec2 inPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Uniforms {
    vec4 pos;     // x, y, w, h
    vec4 props;   // radius, border, shadow, isTex
    vec4 color;   // RGBA
} ubo;

void main() {
    outColor = ubo.color;
    // (Vulkan SPIR-V ready shader logic)
}
