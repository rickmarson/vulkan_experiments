#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "common.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 frag_tex_coord;


layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 1) uniform ModelData {
    mat4 transform;
} model;

void main() {
    mat4 model_view = scene.view * model.transform;
    mat4 proj = scene.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    gl_Position = proj * model_view * vec4(in_position, 1.0);
    frag_color = in_color;
    frag_tex_coord = in_tex_coord;
}
