#pragma once
#include "misc_tools.h"

typedef struct {
    size_t vertex_count;
    size_t index_count;
    VertexInput *vertices;
    IndexInput *indices;
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


#include "font_verts.h"
#include "tryout.h"
GenerateModelOutput load_text_character(StackAllocator *stk_allocr,size_t stk_offset,int codepoint, Vec3 extrude) {
    GenerateModelOutput model = { 0 };
    
    init_font_file("Sanskr.ttf");
    
    //Allocate for CurveNode
    struct AddCurveOutput letter_info =
      add_font_verts(stk_allocr, stk_offset, codepoint);
 

    //Collect shitnodes
    ShitNode *shit_nodes =
      stack_allocate(stk_allocr, &letter_info.stk_offset,
                     letter_info.total_points * sizeof *shit_nodes,
                     sizeof *shit_nodes);

    // Collect in array
    {
        ShitNode *nptr = shit_nodes;
        for (size_t i = 0; i < letter_info.curve_count; ++i) {
            ShitNode *ptr_top = nptr;
            CurveNode *ptr = letter_info.curves[i];
            do {
                nptr->point = ptr->point;
                nptr->flag = 0;
                nptr->children =
                  stack_allocate(stk_allocr, &letter_info.stk_offset,
                                 sizeof *nptr->children, 1);
                nptr->children->next = nptr + 1;
                nptr->children->prev = nptr - 1;
                nptr->children->another = NULL;

                nptr++;
                ptr = ptr->next;

            } while (ptr != letter_info.curves[i]);

            ptr_top->children->prev = nptr - 1;
            nptr[-1].children->next = ptr_top;
        }
    }



    
    //Sort the shitnode curves
    sort_shit_curve(shit_nodes, letter_info.total_points);

    size_t face_triangle_count =
      letter_info.total_points + 2 * letter_info.curve_count - 4;

    //Setup inputs and send
    shitnode_version(
      stk_allocr, letter_info.stk_offset, shit_nodes,
      letter_info.total_points, letter_info.first_point,
      (Triangle *)letter_info.indices, face_triangle_count);

    clear_font_file();

    //Now setup the curve
    //Process each vertex into floating point thing

    //First lwt's add all side curves, so normals won't get messed up
    size_t vert_start = 2 * letter_info.total_points;
    size_t inx_start = 2 * 3 * face_triangle_count;

    for(size_t i = 0; i < letter_info.curve_count; ++i) {

        CurveNode *head = letter_info.curves[i];
        do {

            if (!(head->flag & get_flag(CurveSmooth)) || !(head->next->flag & get_flag(CurveSmooth))) {

                Vec3 p1 = { .x = head->point->point.x,
                            .y = head->point->point.y };
                Vec3 p2 = { .x = head->next->point->point.x,
                            .y = head->next->point->point.y };


                Vec3 norm =
                  vec3_normalize(vec3_cross(vec3_sub(p2,p1), extrude));
                Vec3 neg = vec3_scale_fl(norm, -1.f);

                Vec3 half_ex = vec3_scale_fl(extrude, 0.5f);

                letter_info.first_point[vert_start++].vert =
                  (VertexInput){
                      .pos = vec3_sub(p1, half_ex),
                      .normal = neg,
                  };


                letter_info.first_point[vert_start++].vert =
                  (VertexInput){
                      .pos = vec3_add(p1, half_ex),
                      .normal = neg,
                  };
                
                letter_info.first_point[vert_start++].vert =
                  (VertexInput){
                      .pos = vec3_sub(p2, half_ex),
                      .normal = neg,
                  };

                
                letter_info.first_point[vert_start++].vert =
                  (VertexInput){
                      .pos = vec3_add(p2, half_ex),
                      .normal = neg,
                  };

                letter_info.indices[inx_start++] = vert_start - 4;
                letter_info.indices[inx_start++] = vert_start - 2;
                letter_info.indices[inx_start++] = vert_start - 1;

                letter_info.indices[inx_start++] = vert_start - 1;
                letter_info.indices[inx_start++] = vert_start - 3;
                letter_info.indices[inx_start++] = vert_start - 4;
                
            } else {

                Vec3 half_ex = vec3_scale_fl(extrude, 0.5f);
                Vec3 ex_norm = vec3_normalize(extrude);


                //Push current point only if start of smooth curve
                if ((head == letter_info.curves[i]) || (!(head->prev->flag & get_flag(CurveSmooth)))) {
                    Vec3 p = { head->point->point.x,
                               head->point->point.y };
                    Vec3 n = { head->point->normal.x,
                               head->point->normal.y };

                    n = vec3_sub(
                      n,
                      vec3_scale_fl(ex_norm, vec3_dot(ex_norm, n)));
                    n = vec3_normalize(n);

                    letter_info.first_point[vert_start++].vert =
                      (VertexInput){
                          .pos = vec3_sub(p, half_ex),
                          .normal = n,
                      };

                    letter_info.first_point[vert_start++].vert =
                      (VertexInput){
                          .pos = vec3_add(p, half_ex),
                          .normal = n,
                      };

                }

                //Push next points
                Vec3 p = { .x = head->next->point->point.x,
                            .y = head->next->point->point.y };
                Vec3 n = { head->next->point->normal.x,
                           head->next->point->normal.y };

                n = vec3_sub(
                  n, vec3_scale_fl(ex_norm, vec3_dot(ex_norm, n)));
                n = vec3_normalize(n);

                letter_info.first_point[vert_start++].vert =
                  (VertexInput){
                      .pos = vec3_sub(p, half_ex),
                      .normal = n,
                  };

                letter_info.first_point[vert_start++].vert =
                  (VertexInput){
                      .pos = vec3_add(p, half_ex),
                      .normal = n,
                  };

                
                letter_info.indices[inx_start++] = vert_start - 4;
                letter_info.indices[inx_start++] = vert_start - 2;
                letter_info.indices[inx_start++] = vert_start - 1;

                letter_info.indices[inx_start++] = vert_start - 1;
                letter_info.indices[inx_start++] = vert_start - 3;
                letter_info.indices[inx_start++] = vert_start - 4;
            }
            head = head->next;
            
        } while (head != letter_info.curves[i]);

    }

    //Later also maintain order of vertices, if needed
    //Also maintain normal direction properly
    for(size_t i = 0; i < letter_info.total_points; ++i) {
        letter_info.first_point[i].vert.pos = (Vec3){
            .x =
              -extrude.x * 0.5f + letter_info.first_point[i].point.x,
            .y =
              -extrude.y * 0.5f + letter_info.first_point[i].point.y,
            .z = -extrude.z * 0.5f,
        };
        letter_info.first_point[i].vert.normal = (Vec3){
            .x = letter_info.first_point[i].normal.x,
            .y = letter_info.first_point[i].normal.y,
            .z = 0.f,
        };

        letter_info.first_point[i].vert.normal =
          vec3_scale_fl(vec3_normalize(extrude), -1.f);
        letter_info.first_point[i].vert.uv = (Vec2){ 0.f, 0.f };
    }


    //Duplicate the vertices
    memcpy(letter_info.first_point + letter_info.total_points,
           letter_info.first_point,
           letter_info.total_points *
             sizeof(*letter_info.first_point));

    for (size_t i = letter_info.total_points;
         i < letter_info.total_points*2; ++i) {
        letter_info.first_point[i].vert.pos =
          vec3_add(letter_info.first_point[i].vert.pos, extrude);
        letter_info.first_point[i].vert.uv = (Vec2){ 0.f, 0.f };

        letter_info.first_point[i].vert.normal =
          vec3_scale_fl(vec3_normalize(extrude), 1.f);

    }
    //Rewrite the indices

    //Offset the added indices, and flip two at a time
    for (size_t i = 3 * face_triangle_count;
         i < 2 * 3 * face_triangle_count; i+=3
    ) {
        letter_info.indices[i] = letter_info.total_points + 
          letter_info.indices[i - 3 * face_triangle_count];
        letter_info.indices[i + 2] = letter_info.total_points + 
          letter_info.indices[i + 1 - 3 * face_triangle_count];
        letter_info.indices[i + 1] = letter_info.total_points + 
          letter_info.indices[i + 2 - 3 * face_triangle_count];



    }
    

    model.vertices = letter_info.first_point;
    model.indices = letter_info.indices;
    //model.indices = letter_info.indices + 2 * 3 * face_triangle_count;
    model.index_count = inx_start;
    //model.index_count = 3 * face_triangle_count;
    //model.index_count = inx_start - 2 * 3 * face_triangle_count;
    model.vertex_count = vert_start;

    //
    //   // Shift all x and y pos, and reflect along y
    //for (size_t i = 0; i < model.vertex_count; ++i) {


    //    model.vertices[i].pos.x -= 400;
    //    model.vertices[i].pos.y -= 400;
    //    model.vertices[i].pos.y *= -1;
    //    model.vertices[i].normal.y *= -1;
    //    model.vertices[i].normal.z *= -1;
    //}

    //for (size_t i = 0; i < model.index_count; i+=3) {
    //    swap_stuff(IndexInput, model.indices[i],
    //               model.indices[i + 1]);
    //}


    return model;
}