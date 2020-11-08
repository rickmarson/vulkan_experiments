#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_velocity;

layout(location = 0) out vec4 out_colour;
layout(location = 1) out mat4 out_proj;
layout(location = 5) out uint out_texture_idx;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
} scene;

void main() {
    mat4 model_view = scene.view; // particles are alredy in world coords
    mat4 proj = scene.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    gl_Position = model_view * vec4(in_position.xyz, 1.0);
    out_colour = vec4(1.0, 1.0, 1.0, 1.0);
    out_proj = proj;
    out_texture_idx = 0;
    if (in_position.w > 0.5) out_texture_idx = 1;
}
