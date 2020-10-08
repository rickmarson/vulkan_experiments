#version 450 core
layout(location = 0) out vec4 out_color;

layout(set=0, binding=0) uniform sampler2D fonts_sampler;

layout(location = 0) in struct {
    vec4 color;
    vec2 uv;
} inputs;

void main()
{
    out_color = inputs.color * texture(fonts_sampler, inputs.uv.st);
}
