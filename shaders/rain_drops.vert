#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_velocity;

layout(location = 0) out vec4 out_colour;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 1) uniform ModelData {
    mat4 transform;
} model;

void main() {
    gl_Position = scene.proj * scene.view * model.transform * vec4(in_position.xyz, 1.0);
    out_colour = vec4(1.0, 0.0, 0.0, 1.0);
}