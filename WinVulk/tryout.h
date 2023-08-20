#pragma once


// Actual point union, just hope sizeof vert is larger
union Point {
    VertexInput vert;
    struct {
        POINT point;
        Vec2 normal; // Normal in the xy plane, targeted for beizer
                     // curves
    };
};


//A linked list ds, where node is index to vertex
struct CurveNode {
    struct CurveNode *prev;
    struct CurveNode *next;
    union Point *point;
    UINT flag;

};
typedef struct CurveNode CurveNode;

enum CurveNodeFlag {
    CurveVisit,
    CurveUsed,
    CurveActive,
    CurveSmooth,
};

int get_flag( enum CurveNodeFlag flag ) {
    return 1 << flag;
}

struct Triangle {
    IndexInput a, b, c;
};

typedef struct Triangle Triangle;

#define swap_stuff(type, a, b)  \
    {                           \
        type temporary_variable = (a);         \
        (a) = (b);              \
        (b) = temporary_variable;              \
    }                           \


//Find if a value is in between other two values(inclusive)
#define check_if_in_between(v, a, b)    \
    ((  ((a) <= (b))&&( (v) >= (a)   )&&( (v) <= (b)  )         ) ||\
     (  ((a) >= (b))&&( (v) >= (b)   )&& ((v)<= (a)) ))


#define vec2_cross_prod( v1, v2)       \
    ( ( (v1).x  )  * ( (v2).y  )  - ( (v1).y  ) * ( (v2).x )   )

size_t count_curve_nodes(CurveNode * head)
{
    size_t count = 0;
    CurveNode *ptr = head;
    do
    {
        count++;
        ptr = ptr->next;
    }
    while(ptr != head);
    return count;
}

//A struct as a temporary container, for now, used as a linked list
typedef struct TempStruct
{
    CurveNode *top;
    CurveNode *prev;
    CurveNode *next;

    struct TempStruct *another;
}TempStruct;

//"Foolproof version" hopefully
typedef struct ProcessNode
{
    union Point *point;
    enum CurveNodeFlag flag;
    struct Child
    {
        struct ProcessNode *prev;
        struct ProcessNode *next;
        struct Child *another;
    } *children;

}ProcessNode;


//Only swaps first of children
void swap_process_pts( ProcessNode *a, ProcessNode *b ) {
    
    if((a->children->next == b) && (a->children->prev == b)) {
        swap_stuff( ProcessNode, *a, *b );
        swap_stuff( ProcessNode *, a->children->prev, b->children->prev );
        swap_stuff( ProcessNode *, a->children->next, b->children->next );
    }
    else if((a->children->next == b)) {
        swap_stuff( ProcessNode, *a, *b );
        swap_stuff( ProcessNode *, a->children->prev, b->children->next );
        a->children->next->children->prev = a;
        b->children->prev->children->next = b;
    }
    else if((a->children->prev == b)) {
        swap_stuff( ProcessNode, *a, *b );
        swap_stuff( ProcessNode *, a->children->next, b->children->prev );
        a->children->prev->children->next = a;
        b->children->next->children->prev = b;
    }
    else {
        swap_stuff( ProcessNode, *a, *b );
        a->children->next->children->prev = a;
        a->children->prev->children->next = a;
        b->children->next->children->prev = b;
        b->children->prev->children->next = b;
    }

}

//Collects the curve into the dst in sequential y order
void sort_process_curve( ProcessNode * ptr, size_t count) {
    

    for(size_t i = 0; i < count; ++i) {
        for(size_t j = 0; j < count - i - 1; ++j) {
            if((ptr[j + 1].point->point.y < ptr[j].point->point.y) 
                ||
                ((ptr[j + 1].point->point.y == ptr[j].point->point.y)
                  &&(ptr[j+1].point->point.x < ptr[j].point->point.x)))
                    
            
            {
                swap_process_pts( ptr + j, ptr + j + 1 );
            }
        }
    }

}
#include "misc_tools.h"
#include <Windows.h>
//Now let's try for an array of heads, winding order pre maintained
void triangulate_curve(StackAllocator * stk_allocr, size_t stk_offset,
                            ProcessNode * nodes, size_t total_points,union Point * base_point_ptr, Triangle * triangles, size_t triangle_count)
{
    


    size_t curr_node = 0;
    for(size_t curr_tr = 0; curr_tr < triangle_count; )
    {
        //If curr node is empty, skip it now
        if(nodes[curr_node].children == NULL) {
            curr_node++;
            continue;
        }

        //Case 1 : end point, either of next or prev is upwards, just skip that one
        //Or prev and next are pointing to same, this is also a endcase, trust me

        if((nodes[curr_node].children->prev == nodes[curr_node].children->next) ||
            (((nodes[curr_node].children->prev) < (nodes + curr_node)) ||
            (nodes[curr_node].children->next) < (nodes + curr_node)))
        {
            struct Child *tmp = nodes[curr_node].children->another;
            //free( nodes[curr_node].children );
            nodes[curr_node].children = tmp;
            continue;
        }


        //Else carry out the job

        //Test for triangle in or not
        ProcessNode *greater = ((nodes[curr_node].children->next > nodes[curr_node].children->prev) ? nodes[curr_node].children->next : nodes[curr_node].children->prev);
        ProcessNode *lowest = (ProcessNode*)( - 1);
        for(ProcessNode *ptr = nodes + curr_node + 1; ptr <= greater; ++ptr) {
            if((ptr != nodes[curr_node].children->next) && (ptr != nodes[curr_node].children->prev) ) {
                //Test if inside triangle
                POINT p1, p2, p3;
                p1.x = nodes[curr_node].point->point.x - ptr->point->point.x;
                p1.y = nodes[curr_node].point->point.y - ptr->point->point.y;
                p2.x = nodes[curr_node].children->prev->point->point.x - ptr->point->point.x;
                p2.y = nodes[curr_node].children->prev->point->point.y - ptr->point->point.y;
                p3.x = nodes[curr_node].children->next->point->point.x - ptr->point->point.x;
                p3.y = nodes[curr_node].children->next->point->point.y - ptr->point->point.y;

                LONG v1, v2, v3;
                v1 = vec2_cross_prod( p1, p2 );
                v2 = vec2_cross_prod( p2, p3 );
                v3 = vec2_cross_prod( p3, p1 );

                //Also discard if point is collinear with one of them 
                if (v1 && v2 && v3) {
                    v1 = v1 >= 0;
                    v2 = v2 >= 0;
                    v3 = v3 >= 0;

                    if ((v1 && v2 && v3) || (!v1 && !v2 && !v3)) {
                        if (ptr < lowest)
                            lowest = ptr;
                    }
                }

            }
        }


        if(lowest != -1)
        {
            //Here, break up the curve, don't add any triangles
            struct Child chld1 = {
                .another = nodes[curr_node].children,
                .prev = nodes[curr_node].children->prev,
                .next = lowest,
            };
            struct Child chld2 = {
            .another = nodes[curr_node].children->another,
            .prev = lowest,
            .next = nodes[curr_node].children->next,
            };
            *(nodes[curr_node].children) = chld2;
            nodes[curr_node].children = stack_allocate(
              stk_allocr, &stk_offset, sizeof(chld1), 1);
            *(nodes[curr_node].children) = chld1;

            //Also add these prev and next to the concave point also

            //Here, break up the curve, don't add any triangles
            chld1 = (struct Child){
                .another = lowest->children,
                .prev = lowest->children->prev,
                .next = nodes + curr_node,
            };
            chld2 = (struct Child){
                .another = lowest->children->another,
                .prev = nodes + curr_node,
                .next = lowest->children->next,
            };
            *(lowest->children) = chld2;
            lowest->children = stack_allocate(stk_allocr, &stk_offset,
                                              sizeof(chld1), 1);
            *(lowest->children) = chld1;
            
        }
        else
        {
            //Here, push triangle


            //Maintain cross order also
            if (vec2_cross_prod(
                  nodes[curr_node].point->point,
                  nodes[curr_node].children->next->point->point) -
                  vec2_cross_prod(
                    nodes[curr_node].children->prev->point->point,
                    nodes[curr_node].children->next->point->point)-
                vec2_cross_prod(
                  nodes[curr_node].point->point,
                  nodes[curr_node].children->prev->point->point)) {

                triangles[curr_tr++] = (Triangle){
                    .a = (nodes[curr_node].point - base_point_ptr),
                    .b = (nodes[curr_node].children->prev->point -
                          base_point_ptr),
                    .c = (nodes[curr_node].children->next->point -
                          base_point_ptr),
                };
            }
            else {

                triangles[curr_tr++] = (Triangle){
                    .a = (nodes[curr_node].point - base_point_ptr),
                    .c = (nodes[curr_node].children->prev->point -
                          base_point_ptr),
                    .b = (nodes[curr_node].children->next->point -
                          base_point_ptr),
                };
            }



            //make right left thing make do

            //find from prev es of next that is current, and replace it
            {
                struct Child *chld = nodes[curr_node].children->next->children;
                while(chld->prev != nodes + curr_node)
                {
                    chld = chld->another;
                }
                chld->prev = nodes[curr_node].children->prev;
            }

            //similarly for nexts of prev of current
            {
                struct Child *chld = nodes[curr_node].children->prev->children;
                while(chld->next != nodes + curr_node)
                {
                    chld = chld->another;
                }
                chld->next = nodes[curr_node].children->next;
            }

            //Pop the child
            struct Child *ch = nodes[curr_node].children->another;
            //free( nodes[curr_node].children );
            nodes[curr_node].children = ch;
        }

    }

    //free( nodes );
    //return triangles;

}
