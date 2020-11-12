#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "lighting.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in float depth;
layout(location = 2) in vec3 normal_world;
layout(location = 3) in vec3 position_local;
layout(location = 4) in vec3 light_local;
layout(location = 5) in vec4 light_intensity;
layout(location = 6) in vec4 ambient_intensity;

layout(set = 2, binding = 0) uniform sampler2D diffuse_sampler;
layout(set = 2, binding = 1) uniform sampler2D metal_rough_sampler;
layout(set = 2, binding = 2) uniform sampler2D normal_sampler;

layout(set = 0, binding = 1, rgba32f) uniform image2D scene_depth_buffer;

layout(location = 0) out vec4 out_colour;

void main() {
    ivec2 depth_idx = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    vec4 depth_normal = vec4(depth, normal_world);
    float stored_depth = imageLoad(scene_depth_buffer, depth_idx).x;
    if (depth_normal.x < stored_depth) {
        imageStore(scene_depth_buffer, depth_idx, depth_normal);
    }
    
    vec4 diffuse_colour = texture(diffuse_sampler, frag_tex_coord);
    vec4 metal_rough = texture(metal_rough_sampler, frag_tex_coord);
    vec4 normal_map = texture(normal_sampler, frag_tex_coord);
    vec3 normal = normal_map.xyz * 2.0 - 1.0;

    float metalness = metal_rough.z;
    float roughness = metal_rough.y;

    vec3 frag_color = brdf(position_local, 
                           normal, 
                           diffuse_colour.xyz, 
                           metalness, 
                           roughness, 
                           light_local, 
                           light_intensity.xyz);

    frag_color += ambient_intensity.xyz * diffuse_colour.xyz;
    
    // gamma correction
    frag_color = pow(frag_color, vec3(1.0 / 2.2));
    
    out_colour = vec4(frag_color, diffuse_colour.w);
}
