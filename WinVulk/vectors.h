#pragma once


#define _USE_MATH_DEFINES
#include <math.h>

// Here vertices and vertex data will be defined


union Vec2 {
    float comps[2];
    struct {
        float x;
        float y;
    };
    struct {
        float u;
        float v;
    };
};
typedef union Vec2 Vec2;

union Vec3 {
    float comps[3];
    struct {
        float x;
        float y;
        float z;
    };
    struct {
        float r;
        float g;
        float b;
    };
};
typedef union Vec3 Vec3;

Vec3 vec3_to_radians(Vec3 degrees) {
    return (Vec3){ degrees.x * M_PI / 180.f,
                   degrees.y * M_PI / 180.f,
                   degrees.z * M_PI / 180.f };
}

Vec3 vec3_to_degrees(Vec3 radians) {
    
    return (Vec3){ radians.x * M_1_PI * 180.f,
                   radians.y * M_1_PI * 180.f,
                   radians.z * M_1_PI * 180.f };
}

float vec3_magnitude(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(Vec3 v) {
    float mag = vec3_magnitude(v);
    return (Vec3){ .x = v.x / mag, .y = v.y / mag, .z = v.z / mag };
}

Vec3 vec3_scale_fl(Vec3 v, float s) {
    return (Vec3){ v.x * s, v.y * s, v.z * s };
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        b.x * a.z - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

Vec3 vec3_add_4(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    return (Vec3){
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z,
    };
}

union Vec4 {
    float comps[4];
    struct {
        float x;
        float y;
        float z;
        float w;
    };
    struct {
        float r;
        float g;
        float b;
        float a;
    };
};
typedef union Vec4 Vec4;
typedef const Vec4 CVec4;

Vec4 vec4_from_vec3(Vec3 v, float w) {
    return (Vec4){ .x = v.x, .y = v.y, .z = v.z, .w = w };
}

typedef float Mat4Arr[4][4];
union Mat4 {
    Mat4Arr mat;
    Vec4 cols[4];
    float cells[16];
};
typedef union Mat4 Mat4;
typedef const Mat4 CMat4;

void swap_floats(float *a, float *b) {
    float c = *a;
    *a = *b;
    *b = c;
}

// Creates a transpose matrix, need to cast to Mat4 if not
// initializing
#define mat4_arr_get_transpose(src)                                \
    {{                                                             \
        { src[0][0], src[1][0], src[2][0], src[3][0] },            \
        { src[0][1], src[1][1], src[2][1], src[3][1] },            \
        { src[0][2], src[1][2], src[2][2], src[3][2] },            \
        { src[0][3], src[1][3], src[2][3], src[3][3] }             \
    }}

#define MAT4_IDENTITIY                                             \
    {{                                                             \
        { 1.0f, 0.0f, 0.0f, 0.0f },                                \
        { 0.0f, 1.0f, 0.0f, 0.0f },                                \
        { 0.0f, 0.0f, 1.0f, 0.0f },                                \
        { 0.0f, 0.0f, 0.0f, 1.0f }                                 \
    }}

float vec4_dot_vec( CVec4 a, CVec4 b ) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec4 vec4_scale_fl(CVec4 vec, const float f) {
    return (Vec4){ vec.x * f, vec.y * f, vec.z * f, vec.w * f };
}

Vec4 vec4_scale_vec(CVec4 v1, CVec4 v2) {
    return (Vec4){ v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.w * v2.w };
}

Vec4 vec4_add_vec(CVec4 a, CVec4 b) {
    return (Vec4){ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

Vec4 vec4_sub_vec(CVec4 a, CVec4 b) {
    return (Vec4){ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w };
}

Vec4 vec4_add_vec_4(CVec4 a, CVec4 b, CVec4 c, CVec4 d) {
    return (Vec4){ a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y,
                   a.z + b.z + c.z + d.z, a.w + b.w + c.w + d.w };
}

Vec4 mat4_multiply_vec(CMat4* mat, CVec4 v) {
    return vec4_add_vec_4(vec4_scale_fl(mat->cols[0], v.x),
                          vec4_scale_fl(mat->cols[1], v.y),
                          vec4_scale_fl(mat->cols[2], v.z),
                          vec4_scale_fl(mat->cols[3], v.w));    
}

Mat4 mat4_multiply_mat(CMat4 *a, CMat4 * b ) {

    return (Mat4){ .cols[0] = mat4_multiply_vec(a, b->cols[0]),
                   .cols[1] = mat4_multiply_vec(a, b->cols[1]),
                   .cols[2] = mat4_multiply_vec(a, b->cols[2]),
                   .cols[3] = mat4_multiply_vec(a, b->cols[3]) };

}

Mat4 mat4_multiply_mat_3( CMat4 *a, CMat4 *b, CMat4 * c) {
    Mat4 temp = mat4_multiply_mat(b, c);
    return mat4_multiply_mat(a, &temp);
}


//Form transformation matrices, radians expected
Mat4 mat4_rotation_Z( float angle ) {

    float c = cosf(angle);
    float s = sinf(angle);

    return (Mat4){ { { c, s, 0.f, 0.f },
                     { -s, c, 0.f, 0.f },
                     { 0.f, 0.f, 1.f, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };

}

Mat4 mat4_rotation_X( float angle ) {

    float c = cosf(angle);
    float s = sinf(angle);
    return (Mat4){ { { 1.f, 0.f, 0.f, 0.f },
                     { 0.f, c, s, 0.f },
                     { 0.f, -s, c, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };

}

Mat4 mat4_rotation_Y(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    return (Mat4){ { { c, 0.f, -s, 0.f },
                     { 0.f, 1.f, 0.f, 0.f },
                     { s, 0.f, c, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };

}

Mat4 mat4_rotation_XYZ(Vec3 angles) {
    Mat4 rx = mat4_rotation_X(angles.x);
    Mat4 ry = mat4_rotation_Y(angles.y);
    Mat4 rz = mat4_rotation_Z(angles.z);

    return mat4_multiply_mat_3(&rx, &ry, &rz);
}

Mat4 mat4_scale_3(float sx, float sy, float sz) {
    return (Mat4){ { { sx, 0.f, 0.f, 0.f },
                     { 0.f, sy, 0.f, 0.f },
                     { 0.f, 0.f, sz, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };
}

Mat4 mat4_translate_3(float tx, float ty, float tz) {
    return (Mat4){ { { 1.f, 0.f, 0.f, 0.f },
                     { 0.f, 1.f, 0.f, 0.f },
                     { 0.f, 0.f, 1.f, 0.f },
                     { tx, ty, tz, 1.f } } };
}

//Linearly Maps world min, world max to -1,-1,0 to 1,1,1
Mat4 mat4_orthographic( Vec3 world_min, Vec3 world_max) {

    Vec4 min = { world_min.x, world_min.y, world_min.z };
    Vec4 max = { world_max.x, world_max.y, world_max.z };

    Vec4 sum = vec4_add_vec(min, max);
    Vec4 diff = vec4_sub_vec(max, min);

    sum = vec4_scale_fl(sum, 0.5f);
    diff = vec4_scale_vec(diff, (Vec4){ 0.5f, 0.5f, 1.f, 1.f });

    Mat4 mat1 = mat4_scale_3(1.f/diff.x, 1.f/diff.y, 1/diff.z);
    Mat4 mat2 = mat4_translate_3(-sum.x, -sum.y, -world_min.z);

    return mat4_multiply_mat(&mat1, &mat2);
}

//fov in radians
//Gives a transformation matrix to apply, uses orthographic too in itself
Mat4 mat4_perspective( Vec3 world_min, Vec3 world_max, float fovx) {
    Mat4 ortho = mat4_orthographic(world_min, world_max);
    float D = 1.f / tanf(fovx/2.f);
    Mat4 temp = { {
      { D, 0.f, 0.f, 0.f },
      { 0.f, D, 0.f, 0.f },
      { 0.f, 0.f, 1 + D, 1.f },
      { 0.f, 0.f, 0.f, D },
    } };
    return mat4_multiply_mat(&temp, &ortho);
}

