#version 450
#extension GL_ARB_separate_shader_objects : enable

#define SHOW_BILLBOARDS 0

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D texture_atlas;

void main() {
    vec4 tex_colour = texture(texture_atlas, in_uv);

#if SHOW_BILLBOARDS
    out_color = vec4(mix(frag_color.xyz, tex_colour.xyz, 1.0 - tex_colour.w), 1.0 - tex_colour.w); 
#else
    out_color = vec4(mix(frag_color.xyz, tex_colour.xyz, 0.65), tex_colour.w); 
#endif
    
}
