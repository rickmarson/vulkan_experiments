#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec4 out_colour;
layout(location = 1) out vec2 out_uv;

layout(push_constant) uniform ProjMatrix {
    mat4 proj;
} uproj;

// coordinates inside the rain drops atlas
const vec4 top_left_u = vec4(0.0, 0.5, 0.0, 0.5);
const vec4 top_left_v = vec4(0.0, 0.0, 0.5, 0.5);
const vec4 offsets_u = vec4(0.0, 0.5, 0.0, 0.5);
const vec4 offsets_v = vec4(0.5, 0.5, 0.0, 0.0);

void main() {
    mat4 proj = uproj.proj; 
    projectionToVulkan(proj);

    // particle vertices are alredy in view coords
    gl_Position = proj * vec4(in_position.xyz, 1.0);
    out_colour = vec4(1.0, 1.0, 1.0, 1.0);

    uint texture_idx = 0;
    if (in_position.w > 0.5) texture_idx = 1;

    int offset_idx = gl_VertexIndex - (gl_VertexIndex >> 2) * 4; 
    float u = top_left_u[texture_idx] + offsets_u[offset_idx];
    float v = top_left_v[texture_idx] + offsets_v[offset_idx];
    out_uv = vec2(u, v);
}
