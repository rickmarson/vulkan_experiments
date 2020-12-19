#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal; // unused
layout(location = 2) in vec4 in_tangent; // unused
layout(location = 3) in vec2 in_tex_coord; // unused

layout(set = 0, binding = 0) uniform ShadowMapData {
    mat4 view;
    mat4 proj;
} shadow;

layout(set = 1, binding = 0) uniform ModelData {
    mat4 transform;
} model;

void main() {
    mat4 model_view = shadow.view * model.transform;
    mat4 proj = shadow.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    gl_Position = proj * model_view * vec4(in_position, 1.0);
}
