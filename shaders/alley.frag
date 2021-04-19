#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"
#include "lighting.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in float depth;
layout(location = 2) in vec3 normal_world;
layout(location = 3) in vec3 eye_local;
layout(location = 4) in vec3 light_local;
layout(location = 5) in vec4 light_intensity;
layout(location = 6) in vec4 ambient_intensity;
layout(location = 7) in vec3 shadow_tex_coord;

layout(set = 0, binding = 1) uniform sampler2D scene_textures[18];

layout(set = 2, binding = 0) uniform MaterialData {
    vec3 emissive_factor;
    float metallic_factor;
    float roughness_factor;

    int diffuse_idx;
    int metal_rough_idx;
    int normal_idx;
    int emissive_idx;
} material;

layout(set = 0, binding = 2, rgba32f) uniform image2D scene_depth_buffer;
layout(set = 3, binding = 1) uniform sampler2D shadow_map;

layout(location = 0) out vec4 out_colour;

vec4 emissive_surface() {
    vec4 emissive_colour = texture(scene_textures[material.emissive_idx], frag_tex_coord);
    return vec4(emissive_colour.rgb * material.emissive_factor, 1.0);
}

vec4 lit_surface() {
    vec4 diffuse_colour = texture(scene_textures[material.diffuse_idx], frag_tex_coord);
    vec4 normal_map = texture(scene_textures[material.normal_idx], frag_tex_coord);
    vec3 normal = normalize(normal_map.xyz * 2.0 - 1.0);

    float metalness;
    float roughness;
    if (material.roughness_factor < 0.0) {
        vec4 metal_rough = texture(scene_textures[material.metal_rough_idx], frag_tex_coord);
        metalness = metal_rough.b;
        roughness = metal_rough.g;
    } else {
        metalness = material.metallic_factor;
        roughness = material.roughness_factor;
    }

    vec3 surface_color = brdf( eye_local, 
                            normal, 
                            diffuse_colour.xyz, 
                            metalness, 
                            roughness, 
                            light_local, 
                            light_intensity.xyz);
    
    surface_color += ambient_intensity.xyz * diffuse_colour.xyz;

    return vec4(surface_color, diffuse_colour.w);
}

float shadow_factor() {
    if (texture(shadow_map, shadow_tex_coord.xy).r < shadow_tex_coord.z - 0.0005) {
        return 0.35;
    }
    return 1.0;
}

void main() {
    ivec2 depth_idx = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    vec4 depth_normal = vec4(depth, normal_world);
    float stored_depth = imageLoad(scene_depth_buffer, depth_idx).x;
    if (depth_normal.x < stored_depth) {
        imageStore(scene_depth_buffer, depth_idx, depth_normal);
    }

    if (length(material.emissive_factor) > 0.0) {
        out_colour = emissive_surface();
    } else {
        out_colour = lit_surface() * shadow_factor();
    }
}
