#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(points) in; 
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec4 in_colours[];
layout(location = 1) in mat4 in_proj[];

layout(location = 0) out vec4 out_colour;

// in positions are in the camera view space
const float view_space_offset = 0.025;

void main() {
    
    for(int i = 0; i < gl_in.length(); ++i) {
        mat4 proj = in_proj[i];

        gl_Position = gl_in[i].gl_Position + vec4(-view_space_offset, -view_space_offset, 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        EmitVertex();

        gl_Position = gl_in[i].gl_Position + vec4(view_space_offset, -view_space_offset, 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        EmitVertex();
  
        gl_Position = gl_in[i].gl_Position + vec4(-view_space_offset, view_space_offset, 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        EmitVertex();

        gl_Position = gl_in[i].gl_Position + vec4(view_space_offset, view_space_offset, 0.0, 0.0);
        gl_Position = proj * gl_Position;
        out_colour = in_colours[i];
        EmitVertex();
    }

    EndPrimitive();
}
