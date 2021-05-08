#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_colour;
layout(location = 1) out vec2 out_uv;

layout(set = 0, binding = 0, rgba32f) uniform imageBuffer particle_buffer;

layout(push_constant) uniform ViewProj {
    mat4 view;
    mat4 proj;
} view_proj;

const vec4 top_left_u = vec4(0.0, 0.5, 0.0, 0.5);
const vec4 top_left_v = vec4(0.0, 0.0, 0.5, 0.5);

void main() {
    mat4 model_view = view_proj.view; // particles are alredy in world coords
    mat4 proj = view_proj.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    vec4 centre = imageLoad(particle_buffer, gl_InstanceIndex * 2);
    vec4 centre_view = model_view * vec4(centre.xyz, 1.0);
 
    gl_Position = proj * (centre_view + in_position);

    out_colour = vec4(1.0, 1.0, 1.0, 1.0);
    int texture_idx = 0;
    if (centre.w > 0.5) texture_idx = 1;

    float u = top_left_u[texture_idx] + in_uv.x;
    float v = top_left_v[texture_idx] + in_uv.y;
    out_uv = vec2(u, v);
}
