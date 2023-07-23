#version 450
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 in_vert;
layout(location = 1) in vec3 in_col;
layout(location = 0) out vec4 fragColor;


layout(push_constant) uniform Push{
    mat4 mat;
} push_const;

void main() {

    gl_Position = push_const.mat * vec4(in_vert,1.0);

    //debugPrintfEXT("(%d = %f , %f , %f => %f, %f, %f )",gl_VertexIndex,in_vert.x,in_vert.y,in_vert.z,gl_Position.x, gl_Position.y, gl_Position.z);

    fragColor = vec4(in_col,1.0);
    //fragColor = vec4(gl_Position.z, gl_Position.z, gl_Position.z, 1.0);
}