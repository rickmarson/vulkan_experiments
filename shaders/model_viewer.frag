#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_tex_coord;

layout(set = 2, binding = 0) uniform sampler2D diffuse_sampler;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(diffuse_sampler, frag_tex_coord);
}
