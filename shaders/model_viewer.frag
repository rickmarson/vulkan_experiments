#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_tex_coord;

layout(set = 0, binding = 1) uniform sampler2D scene_textures[1];

layout(set = 2, binding = 0) uniform MaterialData {
    vec3 emissive_factor;
    float metallic_factor;
    float roughness_factor;

    int diffuse_idx;
    int metal_rough_idx;
    int normal_idx;
    int emissive_idx;
} material;


layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(scene_textures[material.diffuse_idx], frag_tex_coord);
    // fix texture color space
    out_color = vec4(pow(out_color.xyz, vec3(2.2)), out_color.w);
}
