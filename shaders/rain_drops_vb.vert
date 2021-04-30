#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec4 out_colour;
layout(location = 1) out vec2 out_uv;

layout(set = 0, binding = 0) uniform ViewProj {
    mat4 view;
    mat4 proj;
} view_proj;

// coordinates inside the rain drops atlas
const vec4 top_left_u = vec4(0.0, 0.5, 0.0, 0.5);
const vec4 top_left_v = vec4(0.0, 0.0, 0.5, 0.5);
const vec4 offsets_u = vec4(0.0, 0.5, 0.0, 0.5);
const vec4 offsets_v = vec4(0.5, 0.5, 0.0, 0.0);

void main() {
    mat4 model_view = view_proj.view; // particles are alredy in world coords
    mat4 proj = view_proj.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    gl_Position = proj * model_view * vec4(in_position.xyz, 1.0);
    out_colour = vec4(1.0, 1.0, 1.0, 1.0);

    uint texture_idx = 0;
    if (in_position.w > 0.5) texture_idx = 1;

    float u = top_left_u[texture_idx] + offsets_u[gl_VertexID % 4];
    float v = top_left_v[texture_idx] + offsets_v[gl_VertexID % 4];
    out_uv = vec2(u, v);
}
