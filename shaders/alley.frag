#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) in float depth;
layout(location = 3) in vec3 normal;

layout(set = 2, binding = 1) uniform sampler2D diffuse_sampler;
layout(set = 0, binding = 1, rgba32f) uniform image2D scene_depth_buffer;

layout(location = 0) out vec4 out_colour;

void main() {
    ivec2 depth_idx = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    vec4 depth_normal = vec4(depth, normal);
    float stored_depth = imageLoad(scene_depth_buffer, depth_idx).x;
    if (depth_normal.x < stored_depth) {
        imageStore(scene_depth_buffer, depth_idx, depth_normal);
    }
    
    out_colour = vec4(frag_color, 1.0) * texture(diffuse_sampler, frag_tex_coord);
}
