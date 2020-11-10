#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out float depth;
layout(location = 3) out vec3 normal;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec3 light_position;
    vec4 light_colour;
} scene;

layout(set = 1, binding = 1) uniform ModelData {
    mat4 transform;
} model;

layout(set = 2, binding = 0) uniform MaterialData {
    vec4 diffuse_factor;
} material;


void main() {
    mat4 model_view = scene.view * model.transform;

    vec3 light_intensity = calcLightIntensity(model_view, 
                                              in_position, 
                                              in_normal, 
                                              scene.light_position, 
                                              scene.light_colour.xyz, 
                                              material.diffuse_factor.xyz);

    mat4 proj = scene.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    gl_Position = proj * model_view * vec4(in_position, 1.0);
    depth = gl_Position.z / gl_Position.w;
    normal = in_normal;
    frag_color = light_intensity;
    frag_tex_coord = in_tex_coord;
}
