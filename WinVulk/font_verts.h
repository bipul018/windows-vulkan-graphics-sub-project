#pragma once

#include "common-stuff.h"

#define STBTT_malloc(size, p_data)                                      \
    ((p_data) ? (stack_allocate((StackAllocator *)((                    \  
                                    (void**)(p_data))[0]),               \
                                (size_t *)((                             \
                                    (void**)(p_data))[1]), (size), 1))    \
              : (((void*)(p_data), malloc(size))))

#define STBTT_free(ptr, p_data)                                      \
    ((p_data) ? ((void)(p_data)) : ((void)(p_data), free(ptr)))

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "tryout.h"
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char *g_font_buffer;
static stbtt_fontinfo g_font_info;

// Pushes a point to given curve, from global point stack
#define PUSH_POINTS(X, Y, CurveInx)                                  \
    curves[(CurveInx)][curve_point_counts[(CurveInx)]].point =       \
      point_store + total_points;                                    \
    curve_point_counts[(CurveInx)]++;                                \
    point_store[total_points].x = (X);                               \
    point_store[total_points].y = (Y);                               \
    total_points++;

// Connects given point with front and back
#define CONNECT_POINT(Curve, Point)                                  \
    curves[(Curve)][(Point)].next = curves[(Curve)] + (Point) + 1;   \
    curves[(Curve)][(Point)].prev = curves[(Curve)] + (Point)-1;


void init_font_file(const char *file_name) {
    static long size;
    FILE *fontFile = fopen(file_name, "rb");
    fseek(fontFile, 0, SEEK_END);
    size = ftell(fontFile); /* how long is the file ? */
    fseek(fontFile, 0, SEEK_SET); /* reset */

    g_font_buffer = (unsigned char *)(malloc(size));

    fread(g_font_buffer, size, 1, fontFile);
    fclose(fontFile);

    /* prepare font */
    if (!stbtt_InitFont(&g_font_info, g_font_buffer, 0)) {
        printf("failed\n");
    }
}

void clear_font_file() { free(g_font_buffer); }


float get_quad_length_integral(POINT P0, POINT Pc, POINT P1,
                               float t) {

    POINT A = {
        .x = P0.x - 2 * Pc.x + P1.x,
        .y = P0.y - 2 * Pc.y + P1.y,
    };

    POINT B = {
        .x = Pc.x - P0.x,
        .y = Pc.y - P0.y,
    };

    POINT C = P0;

    // Now P = At*t + Bt + C

    float sqA = A.x * A.x + A.y * A.y;
    float sqB = B.x * B.x + B.y * B.y;

    float D = A.x * B.x + A.y * B.y;
    D = D / sqA;

    float E = sqB / sqA;

    float K = E - D * D;

    float u = D + t;

    return sqrtf(sqA) *
      (u * sqrtf(u * u + K) + K * logf(u + sqrtf(u * u + K)));
}

Vec2 get_quad_normal(POINT P0, POINT Pc, POINT P1, float t) {
    Vec2 res;
    float u = 1 - t;
    float uu = u * u;
    float tt = t * t;

    res.x = +(uu * P0.y + 2 * t * u * Pc.y + t * t * P1.y);
    res.y = -(uu * P0.x + 2 * t * u * Pc.x + t * t * P1.x);
    return res;
}

// Returns the allocated points, if add_first_point is true, then first point P0
// is also added, and also counted, else not
size_t add_quad_curve(StackAllocator *stk_allocr,
                      size_t *p_stk_offset, POINT P0, POINT Pc,
                      POINT P1, float step_size, float error,
                      CurveNode * end_ptr, union Point *base_pt, CurveNode** p_curr_curve) {
    //end_ptr = NULL => add curve, else don't add
    float ta = 0.f;
    float tb = 1.f;

    float La = get_quad_length_integral(P0, Pc, P1, 0.f);
    float Lb = get_quad_length_integral(P0, Pc, P1, 1.f);

    CurveNode *curr_curve = *p_curr_curve;
    size_t pt_count = 0;

    //Set normal and flag, deciding on the particular normal direction {in or out}
    //should be done later from outside {if is going to be needed}
    curr_curve->flag |= get_flag(CurveSmooth);
    curr_curve->point->normal = get_quad_normal(P0, Pc, P1, 0.f);



    float t0 = ta;
    float t1 = tb;

    float L0 = La;
    float L1 = Lb;

    float t, L;

    // Test for case step_size is too large
    if (step_size < (Lb - La))
        do {

            // Keep using secant till inside 10% of La
            do {
                t =
                  t0 + (t1 - t0) * (La + step_size - L0) / (L1 - L0);
                // Calc L,
                L = get_quad_length_integral(P0, Pc, P1, t);

                if (L < (La + step_size)) {
                    L0 = L;
                    t0 = t;
                } else {
                    L1 = L;
                    t1 = t;
                }


            } while (fabsf(L - (La + step_size)) >=
                     step_size * error);

            if (t >= (1.f))
                break;

            // Interpolate t, to find point
            {
                float t0 = (1 - t) * (1 - t);
                float t1 = t * t;
                float tc = 2 * t * (1 - t);

                POINT P = {
                    .x = roundf(t0 * P0.x + t1 * P1.x + tc * Pc.x ),
                    .y = roundf(t0 * P0.y + t1 * P1.y + tc * Pc.y ),
                };

                // Push point

                curr_curve = stack_allocate(stk_allocr, p_stk_offset,
                                            sizeof(CurveNode),
                                            sizeof(CurveNode));

                base_pt[pt_count].point.x = P.x;
                base_pt[pt_count].point.y = P.y;
                base_pt[pt_count].normal =
                  get_quad_normal(P0, Pc, P1, t);
                curr_curve->flag |= get_flag(CurveSmooth);

                curr_curve->point = base_pt + pt_count;
                pt_count++;

                curr_curve->next = curr_curve + 1;
                curr_curve->prev = curr_curve - 1;
            }

            // Update L values and t values
            ta = t;
            La = L;


        } while (L <= (Lb - step_size * error));

    // Push second endpoint
    if (!end_ptr)
    {

        curr_curve =
          stack_allocate(stk_allocr, p_stk_offset, sizeof(CurveNode),
                         sizeof(CurveNode));

        base_pt[pt_count].point.x = P1.x;
        base_pt[pt_count].point.y = P1.y;
        curr_curve->point = base_pt + pt_count;
        pt_count++;
        end_ptr = curr_curve;

        curr_curve->next = curr_curve + 1;
        curr_curve->prev = curr_curve - 1;
    }
    end_ptr->flag |= get_flag(CurveSmooth);
    end_ptr->point->normal = get_quad_normal(P0, Pc, P1, 1.f);
    (*p_curr_curve) = curr_curve;
    return pt_count;
}


struct AddCurveOutput {
    size_t curve_count;
    size_t total_points;
    CurveNode **curves;
    //First curve node of curves is the overall first curve point
    union Point *first_point;
    IndexInput *indices;
    //Offset of stack allocator after this function is used up
    size_t stk_offset;

};

struct AddCurveOutput add_font_verts(StackAllocator *stk_allocr,
    size_t stk_offset,
    int codepoint) {
    stack_allocate(stk_allocr, &stk_offset, 100, 1);
    stbtt_vertex *p_vertices;

    g_font_info.userdata =
      (void *[]){ (void *)stk_allocr, (void *)&stk_offset };

    
    size_t num_verts =
      stbtt_GetCodepointShape(&g_font_info, codepoint, &p_vertices);


    //Test vertices
    stbtt_vertex test_vertices[] = {

        { .x = -6, .y = 0, .type = STBTT_vmove },
        { .cx = 0, .cy = 10, .x = 5, .y = 0, .type = STBTT_vcurve },
        { .cx = 3, .cy = -10, .x = -6, .y = 0, .type = STBTT_vcurve },
        { .x = 0, .y = -3, .type = STBTT_vmove },
        { .x = 3, .y = 0, .type = STBTT_vline },
        { .cx = -5, .cy = 5, .x = 0, .y = -3, .type = STBTT_vcurve }
    };

    size_t test_vertex_count =
      sizeof(test_vertices) / sizeof(*test_vertices);

    //num_verts = test_vertex_count;
    //memcpy(p_vertices, test_vertices, sizeof(test_vertices));

    g_font_info.userdata = NULL;


    // Count total number of points needed
    size_t num_curves = 0; // Basically number of heads
    size_t num_beizer_points = 0; // Only the internal points
    size_t num_line_points = 0; // No duplicates

    float beizer_error = 0.1f;
    float beizer_steps = 34.f;

    //Also apply some transformations if needed
    for (size_t i = 0; i < num_verts; ++i) {

        //p_vertices[i].y *= -1;
        //p_vertices[i].cy *= -1;
        //p_vertices[i].cy1 *= -1;

        switch (p_vertices[i].type) {
        case STBTT_vmove:
            num_curves++;
            break;
        case STBTT_vline:
            num_line_points++;
            break;
        case STBTT_vcurve: {
            POINT p0 = {
                .x = p_vertices[i - 1].x,
                .y = p_vertices[i - 1].y,
            };
            POINT p1 = {
                .x = p_vertices[i].x,
                .y = p_vertices[i].y,
            };
            POINT pc = {
                .x = p_vertices[i].cx,
                .y = p_vertices[i].cy,
            };
            float l1 = get_quad_length_integral(p0, pc, p1, 0.0f);
            float l2 = get_quad_length_integral(p0, pc, p1, 1.0f);
            num_beizer_points += (size_t)ceilf(
              ceilf((l2 - l1) / beizer_steps) / (1 - beizer_error));
            num_line_points++;
        } break;
        case STBTT_vcubic:
            num_line_points++;
            break;
        default:
            break;
        }
    }

    //Determine a upper limit for everything
    size_t total_face_points =
      num_line_points + num_beizer_points;
    size_t total_side_points =
      num_beizer_points + num_line_points * 2;

    // Allocate for head pointers CurveNode
    CurveNode **curve_heads = stack_allocate(
      stk_allocr, &stk_offset, num_curves * sizeof *curve_heads, 1);

    //Allocate a dummy corruption protection
    stack_allocate(stk_allocr, &stk_offset, 16 * sizeof(void *),
                   sizeof(void *));

    //Allocate the upper limit to vertices for GPU and processing
    union Point *points =
      stack_allocate(stk_allocr, &stk_offset,
                     sizeof(union Point) *
                       (total_face_points + total_side_points) * 2,
                     sizeof(union Point));
    size_t total_face_triangles =
      total_face_points + 2 * num_curves - 4;
    size_t total_side_triangles = 2 * total_face_points;

    //Allocate the upper limit for triangles {later used as indices}
    IndexInput *indices = stack_allocate(
      stk_allocr, &stk_offset,
      sizeof(IndexInput) *
        (total_face_triangles*2 + total_side_triangles) * 3,
      sizeof(IndexInput));


    //Allocate for curve nodes now, but do this in stack allocator uninterrupted
    //So all of it is linearly allocated

    size_t curve_inx = -1;
    size_t point_inx = 0;
    CurveNode *curr_curve = NULL;


    for (size_t i = 0; i < num_verts; ++i) {

        int curve_end =
          (i == (num_verts-1)) || (p_vertices[i + 1].type == STBTT_vmove);

        switch (p_vertices[i].type) {
        case STBTT_vmove: {

            curve_inx++;

            curr_curve =
              stack_allocate(stk_allocr, &stk_offset,
                             sizeof(CurveNode), sizeof(CurveNode));

            curve_heads[curve_inx] = curr_curve;

            points[point_inx].point.x = p_vertices[i].x;
            points[point_inx].point.y = p_vertices[i].y;

            curr_curve->point = points + point_inx;
            point_inx++;
            curr_curve->flag = 0;
            curr_curve->next = curr_curve + 1;
            curr_curve->prev = curr_curve - 1;


            break;
        }
        case STBTT_vline: {
            if (curve_end) {

                // Join up curve end points
                curr_curve->next = curve_heads[curve_inx];
                curve_heads[curve_inx]->prev = curr_curve;


            } else {

                curr_curve = stack_allocate(stk_allocr, &stk_offset,
                                            sizeof(CurveNode),
                                            sizeof(CurveNode));

                points[point_inx].point.x = p_vertices[i].x;
                points[point_inx].point.y = p_vertices[i].y;

                curr_curve->point = points + point_inx;
                point_inx++;
                curr_curve->flag = 0;
                curr_curve->next = curr_curve + 1;
                curr_curve->prev = curr_curve - 1;
            }

            break;
        }

        case STBTT_vcurve: {
            POINT pa, pb, pc;
            pa.x = p_vertices[i - 1].x;
            pa.y = p_vertices[i - 1].y;

            pb.x = p_vertices[i].x;
            pb.y = p_vertices[i].y;

            pc.x = p_vertices[i].cx;
            pc.y = p_vertices[i].cy;


            // Case if this is first point on the curve

            size_t added_points = add_quad_curve(
              stk_allocr, &stk_offset, pa, pc, pb, beizer_steps,
              beizer_error,
              (curve_end) ? (curve_heads[curve_inx]) : NULL,
              points + point_inx, &curr_curve);

            point_inx += added_points;
            if (curve_end) {
                // Join up curve end points
                curr_curve->next = curve_heads[curve_inx];
                curve_heads[curve_inx]->prev = curr_curve;
            }

            break;
        }

        case STBTT_vcubic: {
            if (curve_end) {

                // Join up curve end points
                curr_curve->next = curve_heads[curve_inx];
                curve_heads[curve_inx]->prev = curr_curve;


            } else {

                curr_curve = stack_allocate(stk_allocr, &stk_offset,
                                            sizeof(CurveNode),
                                            sizeof(CurveNode));

                points[point_inx].point.x = p_vertices[i].x;
                points[point_inx].point.y = p_vertices[i].y;

                curr_curve->point = points + point_inx;
                point_inx++;
                curr_curve->flag = 0;
                curr_curve->next = curr_curve + 1;
                curr_curve->prev = curr_curve - 1;
            }
        }
        }
    }

    struct AddCurveOutput output;
    output.first_point = points;
    output.curves = curve_heads;
    output.curve_count = num_curves;
    output.indices = indices;
    output.total_points = point_inx;
    output.stk_offset = stk_offset;
    return output;
}

