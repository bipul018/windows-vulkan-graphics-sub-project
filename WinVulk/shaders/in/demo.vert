#version 450
//#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec2 verts;
layout(location = 0) out vec3 fragColor;


layout(push_constant) uniform Push{
    float del;
} push_const;

//layout(binding = 0) uniform Push{
//    float del;
//} push_const;

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.3, 0.0, 1.0),
    vec3(0.0, 1.0, 0.0) 
);
vec2 sample_data[6] = {
        { -0.5, -0.5 }, { 0.5, 0.5 },   { -0.5, 0.5 },
        { 0.5, 0.5 },   { -0.5, -0.5 }, { 0.5, -0.5 }
    };
void main() {
    //debugPrintfEXT("(%d = %f : %f)",gl_VertexIndex,verts.x,verts.y);
    //if(gl_VertexIndex % 3 == 0)
    //    debugPrintfEXT("\n");
    
    vec2 delta = vec2(push_const.del,0.0);
    gl_Position = vec4(verts + delta, 0.0, 1.0);
    //gl_Position = vec4(sample_data[gl_VertexIndex] + delta, 0.0, 1.0);
    fragColor = colors[gl_VertexIndex%3];
}