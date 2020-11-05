#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) in float depth;

layout(set = 2, binding = 1) uniform sampler2D diffuse_sampler;
layout(set = 0, binding = 1, r32f) uniform image2D scene_depth_buffer;

layout(location = 0) out vec4 out_color;

void main() {
    ivec2 depth_idx = ivec2(gl_FragCoord.x, gl_FragCoord.y);
    vec4 depth_r32 = vec4(depth, 0.0, 0.0, 0.0);
    vec4 stored_depth = imageLoad(scene_depth_buffer, depth_idx);
    if (depth_r32.x < stored_depth.x) {
        imageStore(scene_depth_buffer, depth_idx, depth_r32);
    }
    
    out_color = vec4(frag_color, 1.0) * texture(diffuse_sampler, frag_tex_coord);
    //out_color = vec4(depth, depth, depth, 1.0);
}
