#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

// at the moment the scene-level uniforms must be the same for all pipelines in a program.
layout(set = 0, binding = 1, r32f) uniform image2D scene_depth_buffer;

void main() {
    out_color = frag_color;
}
