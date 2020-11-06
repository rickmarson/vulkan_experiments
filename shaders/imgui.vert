#version 450 core
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in uint in_color;

layout(push_constant) uniform UiTransform {
    vec2 scale;
    vec2 translate;
} transform;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out struct {
    vec4 color;
    vec2 uv;
} outputs;

void main()
{
    outputs.color = vec4(
        ((in_color >> 0) & 0xFF) / 255.0,
        ((in_color >> 8) & 0xFF) / 255.0,
        ((in_color >> 16) & 0xFF) / 255.0,
        ((in_color >> 24) & 0xFF) / 255.0
    );

    outputs.uv = in_uv;
    gl_Position = vec4(in_position * transform.scale + transform.translate, 0, 1);
}
