#pragma once
#include "misc_tools.h"


typedef struct {
    size_t vertex_count;
    size_t index_count;
    VertexInput *vertices;
    uint16_t *indices;
} GenerateModelOutput;
GenerateModelOutput load_sphere_uv(StackAllocator *stk_allocr,
                                   size_t stk_offset,
                                   size_t num_parts, float radius) {

    GenerateModelOutput out;

    // Num of no_sides in a circle/ longitude lines
    size_t no_sides = 4 * num_parts;
    // Num of latitude lines, except for poles
    size_t no_lats = 2 * num_parts - 1;
    out.vertex_count = 2 + no_lats * no_sides;

    // Number of triangles, at the poles only
    size_t n_triangles = 2 * no_sides;
    // Number of quads , not at the poles
    size_t n_quads = (no_lats - 1) * no_sides;
    out.index_count = 3 * (2 * n_quads + n_triangles);

    // Allocate the vertices and indices in stk_allocr
    out.vertices =
      stack_allocate(stk_allocr, &stk_offset,
                     sizeof(VertexInput) * out.vertex_count, 1);
    out.indices = stack_allocate(
      stk_allocr, &stk_offset, sizeof(uint16_t) * out.index_count, 1);

    if (!out.vertices || !out.indices)
        return (GenerateModelOutput){ 0 };

    // Current vertex tracker
    VertexInput *curr_vert = out.vertices;

    // Create north pole
    {
        curr_vert->pos = (Vec3){ 0.f, 0.f, radius };
        curr_vert++;
    }

    // Create the latitudes' circles
    for (int i = 1; i <= no_lats; ++i) {

        // Lets do some wave thing
        // float R =
        //   radius + (rand() / (1.f + RAND_MAX) - 0.5f) * radius*0.1;

        float phi = i * M_PI / (no_lats + 1);
        float cosphi = cosf(phi);
        float sinphi = sinf(phi);
        float z = radius * cosphi;
        float r = radius * sinphi;

        // Create the circle
        for (int j = 0; j < no_sides; ++j) {
            float theta = j * M_PI * 2.f / no_sides;
            float x = r * cosf(theta);
            float y = r * sinf(theta);
            curr_vert->pos = (Vec3){ x, y, z };
            curr_vert++;
        }
    }

    // Create the south pole
    {
        curr_vert->pos = (Vec3){ 0.f, 0.f, -radius };
        curr_vert++;
    }

    // Fill the normals by just normalizing the vertices
    for (int i = 0; i < out.vertex_count; ++i) {
        out.vertices[i].normal = vec3_normalize(out.vertices[i].pos);
        out.vertices[i].uv = (Vec2){ 0.f, 0.f };
    }

    uint16_t *curr_inx = out.indices;

    // Fill the north & south pole triangles & quads
    {
        size_t north_pole = 0;
        size_t south_pole = out.vertex_count - 1;

        // Run for each latitude lines , but only for one hemisphere
        for (int i = 0; i < num_parts; ++i) {
            // Now run for each part in the current latitude circle

            for (int j = 0; j < no_sides; ++j) {

                // Current point for north hemisphere {from
                // beginning//Increasing order}
                size_t curr_north_inx =
                  north_pole + 1 + i * no_sides + j;
                // Current point for south hemisphere {from
                // last//Decreasing order}
                size_t curr_south_inx =
                  south_pole - 1 - i * no_sides - j;

                // Next point for north hemisphere in same latitude
                // circle
                size_t next_north_inx =
                  north_pole + 1 + i * no_sides + (j + 1) % no_sides;
                // Next point for south hemisphere in same latitude
                // circle {reverse order}
                size_t next_south_inx =
                  south_pole - 1 - i * no_sides - (j + 1) % no_sides;

                // Case when triangles are to be formed
                if (i == 0) {
                    // Each current north/south hemisphere point makes
                    // a triangle with north/south pole along with
                    // next corresponding points

                    curr_inx[0] = north_pole;
                    curr_inx[1] = next_north_inx;
                    curr_inx[2] = curr_north_inx;
                    curr_inx += 3;

                    curr_inx[0] = south_pole;
                    curr_inx[1] = next_south_inx;
                    curr_inx[2] = curr_south_inx;
                    curr_inx += 3;
                }

                // Case when quads are to be formed
                else {
                    // Each current north/south hemisphere point ,
                    // along with next points, make quads with
                    // previous/next lattitude lines of same longitude
                    // Each longitude points are no_sides vertices
                    // spaced

                    curr_inx[0] = next_north_inx;
                    curr_inx[1] = curr_north_inx;
                    curr_inx[2] = next_north_inx - no_sides;

                    curr_inx[3] = next_north_inx - no_sides;
                    curr_inx[4] = curr_north_inx;
                    curr_inx[5] = curr_north_inx - no_sides;
                    curr_inx += 6;

                    curr_inx[0] = next_south_inx;
                    curr_inx[1] = curr_south_inx;
                    curr_inx[2] = next_south_inx + no_sides;

                    curr_inx[3] = curr_south_inx + no_sides;
                    curr_inx[4] = next_south_inx + no_sides;
                    curr_inx[5] = curr_south_inx;
                    curr_inx += 6;
                }
            }
        }
    }


    return out;
}

GenerateModelOutput load_cuboid_aa(StackAllocator *stk_allocr,
                                   size_t stk_offset, Vec3 dims) {

    GenerateModelOutput out = { 0 };
    out.vertex_count = 6 * 4;
    out.index_count = 6 * 2 * 3;
    out.vertices =
      stack_allocate(stk_allocr, &stk_offset,
                     out.vertex_count * sizeof *out.vertices, 1);
    out.indices =
      stack_allocate(stk_allocr, &stk_offset,
                     out.index_count * sizeof *out.indices, 1);

    Vec3 *temp =
      stack_allocate(stk_allocr, &stk_offset, 8 * sizeof *temp, 1);

    if (!out.indices || !out.vertices)
        return (GenerateModelOutput){ 0 };

    // Fill temp only for now
    //Fills in order : 0,x,y,xy,z,zx,zy,zxy
    for(int i = 0; i < 8; ++i) {
        temp[i] = (Vec3){ -dims.x / 2, -dims.y / 2, -dims.z / 2 };
        temp[i] = vec3_add(temp[i],
                           (Vec3){
                             .x = ((i & 0b001) ? dims.x : 0.f),
                             .y = ((i & 0b010) ? dims.y : 0.f),
                             .z = ((i & 0b100) ? dims.z : 0.f),
                           });
    }
    
    VertexInput *vert = out.vertices;

    //Front face
    vert[0].pos = temp[0];
    vert[1].pos = temp[1];
    vert[2].pos = temp[3];
    vert[3].pos = temp[2];
    vert[0].normal = vert[1].normal = vert[2].normal =
      vert[3].normal = (Vec3){ 0.f, 0.f, -1.f };

    vert += 4;


    //Back face
    vert[0].pos = temp[5];
    vert[1].pos = temp[4];
    vert[2].pos = temp[6];
    vert[3].pos = temp[7];
    vert[0].normal = vert[1].normal = vert[2].normal =
      vert[3].normal = (Vec3){ 0.f, 0.f, 1.f };
    vert += 4;

    //Right face
    vert[0].pos = temp[1];
    vert[1].pos = temp[5];
    vert[2].pos = temp[7];
    vert[3].pos = temp[3];
    vert[0].normal = vert[1].normal = vert[2].normal =
      vert[3].normal = (Vec3){ 1.f, 0.f, 0.f };
    vert += 4;

    //Left face
    vert[0].pos = temp[4];
    vert[1].pos = temp[0];
    vert[2].pos = temp[2];
    vert[3].pos = temp[6];
    vert[0].normal = vert[1].normal = vert[2].normal =
      vert[3].normal = (Vec3){ -1.f, 0.f, 0.f };
    vert += 4;

    //Top face
    vert[0].pos = temp[4];
    vert[1].pos = temp[5];
    vert[2].pos = temp[1];
    vert[3].pos = temp[0];
    vert[0].normal = vert[1].normal = vert[2].normal =
      vert[3].normal = (Vec3){ 0.f, -1.f, 0.f };
    vert += 4;


    //Bottom face
    vert[0].pos = temp[2];
    vert[1].pos = temp[3];
    vert[2].pos = temp[7];
    vert[3].pos = temp[6];
    vert[0].normal = vert[1].normal = vert[2].normal =
      vert[3].normal = (Vec3){ 0.f, 1.f, 0.f };
    vert += 4;

    for(int i = 0; i < 6; ++i) {
        out.indices[i * 6 + 0] = i * 4 + 0;
        out.indices[i * 6 + 1] = i * 4 + 1;
        out.indices[i * 6 + 2] = i * 4 + 2;
        
        out.indices[i * 6 + 3] = i * 4 + 2;
        out.indices[i * 6 + 4] = i * 4 + 3;
        out.indices[i * 6 + 5] = i * 4 + 0;

    }
    return out;

}