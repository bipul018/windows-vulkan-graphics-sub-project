#version 450

layout(location = 0) out vec3 fragColor;
layout(location = 0) in vec2 verts;


//layout(push_constant) uniform Push{
layout(binding = 0) uniform Push{
    float del;
} push_const;

//
//vec2 positions[3] = vec2[](
//    vec2(0.0, -0.5),
//    vec2(0.5, 0.5),
//    vec2(-0.5, 0.5)
//);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.3, 0.0, 1.0),
    vec3(0.0, 1.0, 0.0)
);

void main() {
    vec2 delta = vec2(push_const.del,0.0);
    gl_Position = vec4(verts + delta, 0.0, 1.0);
    fragColor = colors[gl_VertexIndex%3];
}