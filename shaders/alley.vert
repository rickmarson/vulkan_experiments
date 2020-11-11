#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out float depth;
layout(location = 2) out vec3 normal_world;
layout(location = 3) out vec3 position_view;
layout(location = 4) out vec3 normal_view;
layout(location = 5) out vec3 light_view;
layout(location = 6) out vec4 light_intensity;
layout(location = 7) out vec4 ambient_intensity;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    vec3 light_position;
    vec4 light_intensity;
    vec4 ambient_intensity;
} scene;

layout(set = 1, binding = 1) uniform ModelData {
    mat4 transform;
} model;

void main() {
    mat4 model_view = scene.view * model.transform;
    mat4 proj = scene.proj;
    worldToVulkan(model_view);
    projectionToVulkan(proj);

    vec4 position_view4 = model_view * vec4(in_position, 1.0);

    // save out the vectors needed for light calculations
    position_view = position_view4.xyz;
    light_view = (model_view * vec4(scene.light_position, 1.0)).xyz;

    // note: this is not strictly correct (should be transpose(inverse(model_view))),
	// but since we are not using non-uniform scaling it's an acceptable approximation
    normal_view = normalize(mat3(model_view) * in_normal);
    light_intensity = scene.light_intensity;
    ambient_intensity = scene.ambient_intensity;
    
    // standard MVP transform
    gl_Position = proj * position_view4;
    frag_tex_coord = in_tex_coord;

    // save out depth and world normal for the custom depth buffer
    depth = gl_Position.z / gl_Position.w;
    normal_world = in_normal;   
}
