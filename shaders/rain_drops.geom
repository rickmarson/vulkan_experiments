#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec4 in_colours[];
layout(location = 0) out vec4 out_colour;

void main() {
    
    for(int i = 0; i < gl_in.length(); ++i) {
        gl_Position = gl_in[i].gl_Position + vec4(-0.05, -0.05, 0.0, 0.0);
        out_colour = in_colours[i];
        EmitVertex();

        gl_Position = gl_in[i].gl_Position + vec4(-0.05, 0.05, 0.0, 0.0);
        out_colour = in_colours[i];
        EmitVertex();
  
        gl_Position = gl_in[i].gl_Position + vec4(0.05, -0.05, 0.0, 0.0);
        out_colour = in_colours[i];
        EmitVertex();

        gl_Position = gl_in[i].gl_Position + vec4(0.05, 0.05, 0.0, 0.0);
        out_colour = in_colours[i];
        EmitVertex();
    }

    EndPrimitive();
}
