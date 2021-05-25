#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(points) in; 
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec4 in_colours[];
layout(location = 1) in mat4 in_proj[];
layout(location = 5) in uint in_texture_idx[];

layout(location = 0) out vec4 out_colour;
layout(location = 1) out vec2 out_uv;

// in positions are in the camera view space
const vec4 half_size_x = vec4(0.005, 0.065, 0.05, 0.06);
const vec4 half_size_y = vec4(0.08, 0.065, 0.05, 0.06);
const vec4 top_left_u = vec4(0.0, 0.5, 0.0, 0.5);
const vec4 top_left_v = vec4(0.0, 0.0, 0.5, 0.5);

void main() {
    
    for(int i = 0; i < gl_in.length(); ++i) {
        mat4 proj = in_proj[i];
        uint tex_idx = in_texture_idx[i];
        vec4 billboard_offset = vec4(0.0, -half_size_y[tex_idx], 0.0, 0.0);
        vec4 particle_pos = gl_in[i].gl_Position;
        vec4 pos = particle_pos + billboard_offset;

        gl_Position = pos + vec4(-half_size_x[tex_idx], -half_size_y[tex_idx], 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        out_uv = vec2(top_left_u[tex_idx], top_left_v[tex_idx] + 0.5);
        EmitVertex();

        gl_Position = pos + vec4(half_size_x[tex_idx], -half_size_y[tex_idx], 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        out_uv = vec2(top_left_u[tex_idx] + 0.5, top_left_v[tex_idx] + 0.5);
        EmitVertex();
  
        gl_Position = pos + vec4(-half_size_x[tex_idx], half_size_y[tex_idx], 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        out_uv = vec2(top_left_u[tex_idx], top_left_v[tex_idx]);
        EmitVertex();

        gl_Position = pos + vec4(half_size_x[tex_idx], half_size_y[tex_idx], 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        out_uv = vec2(top_left_u[tex_idx] + 0.5, top_left_v[tex_idx]);
        EmitVertex();
    }

    EndPrimitive();
}
