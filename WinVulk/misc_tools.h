#pragma once
#include "device-mem-stuff.h"
#include "render-stuff.h"
#include "vectors.h"


// Currently used push constants
struct PushConst {
    Mat4 view_proj;
    Mat4 model_mat;
};

const static VkPushConstantRange push_ranges[] = {
    {
      .offset = 0,
      .size = sizeof(struct PushConst),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    },
};

// Currently used shader input vertex
struct VertexInput {
    Vec3 pos;
    Vec3 normal;
    Vec3 color;
    Vec2 uv;
};
typedef struct VertexInput VertexInput;

const static VkVertexInputBindingDescription binding_descs[] = { {
  .binding = 0,
  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  .stride = sizeof(VertexInput),
} };

const static VkVertexInputAttributeDescription attrib_descs[] = {
    {
      .binding = 0,
      .location = 0,
      .offset = offsetof(VertexInput, pos),
      .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    {
      .binding = 0,
      .location = 1,
      .offset = offsetof(VertexInput, normal),
      .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    {
      .binding = 0,
      .location = 2,
      .offset = offsetof(VertexInput, color),
      .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    {
      .binding = 0,
      .location = 3,
      .offset = offsetof(VertexInput, uv),
      .format = VK_FORMAT_R32G32_SFLOAT,
    }
};

// Based on currently used vertices and indices
struct Model {
    size_t vertex_count;
    size_t indices_count;
    GpuAllocrAllocatedBuffer vert_buffer;
    GpuAllocrAllocatedBuffer index_buffer;
};

enum CreateModelCodes {

    CREATE_MODEL_DUMMY_FAIL = -0x7fff,
    CREATE_MODEL_INDEX_COPY_FAIL,
    CREATE_MODEL_VERTEX_COPY_FAIL,
    CREATE_MODEL_INDEX_ALLOC_FAIL,
    CREATE_MODEL_VERTEX_ALLOC_FAIL,
    CREATE_MODEL_INCOMPATIBLE_MEMORY,
    CREATE_MODEL_INDEX_BUFFER_CREATE_FAIL,
    CREATE_MODEL_VERTEX_BUFFER_CREATE_FAIL,
    CREATE_MODEL_OK = 0,
};
typedef struct {
    VkDevice device;
    GPUAllocator *p_allocr;

    size_t vertex_count;
    size_t index_count;

    VertexInput *vertices_list;
    uint16_t *indices_list;
} CreateModelParam;
int create_model(VkAllocationCallbacks *alloc_callbacks,
                 CreateModelParam param, struct Model *out_model) {

    if (vkCreateBuffer(
          param.device,
          &(VkBufferCreateInfo){
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = sizeof(*param.vertices_list) * param.vertex_count,
          },
          alloc_callbacks,
          &out_model->vert_buffer.buffer) != VK_SUCCESS)
        return CREATE_MODEL_VERTEX_BUFFER_CREATE_FAIL;

    if (vkCreateBuffer(
          param.device,
          &(VkBufferCreateInfo){
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = sizeof(*param.indices_list) * param.index_count,
          },
          alloc_callbacks,
          &out_model->index_buffer.buffer) != VK_SUCCESS)
        return CREATE_MODEL_INDEX_BUFFER_CREATE_FAIL;

    out_model->indices_count = param.index_count;
    out_model->vertex_count = param.vertex_count;

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(
      param.device, out_model->vert_buffer.buffer, &mem_reqs);

    out_model->vert_buffer.base_align = mem_reqs.alignment;
    out_model->vert_buffer.total_amt = mem_reqs.size;

    uint32_t mem_flag = mem_reqs.memoryTypeBits;

    vkGetBufferMemoryRequirements(
      param.device, out_model->index_buffer.buffer, &mem_reqs);

    out_model->index_buffer.base_align = mem_reqs.alignment;
    out_model->index_buffer.total_amt = mem_reqs.size;
    mem_flag = mem_flag & mem_reqs.memoryTypeBits;


    if ((mem_flag & param.p_allocr->memory_type) == 0)
        return CREATE_MODEL_INCOMPATIBLE_MEMORY;

    if ((gpu_allocr_allocate_buffer(param.p_allocr, param.device,
                                    &out_model->vert_buffer) < 0) ||
        !out_model->vert_buffer.mapped_memory)
        return CREATE_MODEL_VERTEX_ALLOC_FAIL;
    if ((gpu_allocr_allocate_buffer(param.p_allocr, param.device,
                                    &out_model->index_buffer) < 0) ||
        !out_model->index_buffer.mapped_memory)
        return CREATE_MODEL_INDEX_ALLOC_FAIL;

    memcpy(out_model->vert_buffer.mapped_memory, param.vertices_list,
           sizeof(*param.vertices_list) * param.vertex_count);

    if (gpu_allocr_flush_memory(param.device, *param.p_allocr,
                                out_model->vert_buffer.mapped_memory,
                                out_model->vert_buffer.total_amt) !=
        VK_SUCCESS)
        return CREATE_MODEL_VERTEX_COPY_FAIL;

    memcpy(out_model->index_buffer.mapped_memory, param.indices_list,
           sizeof(*param.indices_list) * param.index_count);

    if (gpu_allocr_flush_memory(param.device, *param.p_allocr,
                                out_model->index_buffer.mapped_memory,
                                out_model->index_buffer.total_amt) !=
        VK_SUCCESS)
        return CREATE_MODEL_INDEX_COPY_FAIL;


    return CREATE_MODEL_OK;
}

void clear_model(VkAllocationCallbacks *alloc_callbacks,
                 VkDevice device, struct Model *p_model) {
    if (p_model->indices_count || p_model->vertex_count) {
        if (p_model->vert_buffer.buffer)
            vkDestroyBuffer(device, p_model->vert_buffer.buffer,
                            alloc_callbacks);
        if (p_model->index_buffer.buffer)
            vkDestroyBuffer(device, p_model->index_buffer.buffer,
                            alloc_callbacks);
    }
    *p_model = (struct Model){ 0 };
}

void submit_model_draw(struct Model *p_model,
                       VkCommandBuffer cmd_buffer) {

    vkCmdBindVertexBuffers(cmd_buffer, 0, 1,
                           &p_model->vert_buffer.buffer,
                           (VkDeviceSize[]){ 0 });
    vkCmdBindIndexBuffer(cmd_buffer, p_model->index_buffer.buffer, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buffer, p_model->indices_count, 1, 0, 0, 0);
    //vkCmdDraw(cmd_buffer, p_model->vertex_count, 1, 0, 0);
}

typedef struct {
    size_t vertex_count;
    size_t index_count;
    VertexInput *vertices;
    uint16_t *indices;
} LoadSphereOutput;
LoadSphereOutput load_sphere_uv( StackAllocator *stk_allocr,
                                 size_t stk_offset, size_t num_parts,
                                 float radius ) {

    LoadSphereOutput out;

    //Num of no_sides in a circle/ longitude lines
    size_t no_sides = 4 * num_parts;
    //Num of latitude lines, except for poles
    size_t no_lats = 2 * num_parts - 1;
    out.vertex_count = 2 + no_lats * no_sides;

    //Number of triangles, at the poles only
    size_t n_triangles = 2 * no_sides;
    //Number of quads , not at the poles
    size_t n_quads =( no_lats - 1 )* no_sides;
    out.index_count = 3 * (2 * n_quads + n_triangles);

    //Allocate the vertices and indices in stk_allocr
    out.vertices =
      stack_allocate(stk_allocr, &stk_offset,
                     sizeof(VertexInput) * out.vertex_count, 1);
    out.indices = stack_allocate(
      stk_allocr, &stk_offset, sizeof(uint16_t) * out.index_count, 1);

    if (!out.vertices || !out.indices)
        return (LoadSphereOutput){ 0 };

    //Current vertex tracker
    VertexInput *curr_vert = out.vertices;

    //Create north pole
    {
        curr_vert->pos = (Vec3){ 0.f, 0.f, radius };
        curr_vert++;
    }

    //Create the latitudes' circles
    for (int i = 1; i <= no_lats; ++i) { 

        //Lets do some wave thing
        //float R =
        //  radius + (rand() / (1.f + RAND_MAX) - 0.5f) * radius*0.1;

        float phi = i * M_PI / (no_lats + 1);
        float cosphi = cosf(phi);
        float sinphi = sinf(phi);
        float z = radius * cosphi;
        float r = radius * sinphi;

        //Create the circle
        for(int j = 0; j < no_sides; ++j) {
            float theta = j * M_PI * 2.f / no_sides;
            float x = r * cosf(theta);
            float y = r * sinf(theta);
            curr_vert->pos = (Vec3){ x, y, z };
            curr_vert++;
        }

    }

    //Create the south pole
    {
        curr_vert->pos = (Vec3){ 0.f, 0.f, -radius };
        curr_vert++;
    }

    //Fill the normals by just normalizing the vertices
    for(int i = 0; i < out.vertex_count; ++i) {
        out.vertices[i].normal = vec3_normalize(out.vertices[i].pos);
        out.vertices[i].color =
          (Vec3){ .r = 0.5f, .g = 0.5f, .b = 0.9f };
        out.vertices[i].uv = (Vec2){ 0.f, 0.f };
    }

    uint16_t *curr_inx = out.indices;

    //Fill the north & south pole triangles & quads
    {
        size_t north_pole = 0;
        size_t south_pole = out.vertex_count - 1;
        
        //Run for each latitude lines , but only for one hemisphere
        for(int i = 0; i < num_parts; ++i) {
            //Now run for each part in the current latitude circle

            for(int j = 0; j < no_sides; ++j) {

                //Current point for north hemisphere {from beginning//Increasing order}
                size_t curr_north_inx = north_pole + 1 + i * no_sides + j;
                //Current point for south hemisphere {from last//Decreasing order}
                size_t curr_south_inx = south_pole - 1 - i * no_sides - j;

                //Next point for north hemisphere in same latitude circle
                size_t next_north_inx =
                  north_pole + 1 + i * no_sides + (j + 1) % no_sides;
                //Next point for south hemisphere in same latitude circle {reverse order}
                size_t next_south_inx =
                  south_pole - 1 - i * no_sides - (j + 1) % no_sides;

                //Case when triangles are to be formed
                if(i == 0){
                    //Each current north/south hemisphere point makes a triangle with north/south pole
                    //along with next corresponding points

                    curr_inx[0] = north_pole;
                    curr_inx[1] = next_north_inx;
                    curr_inx[2] = curr_north_inx;
                    curr_inx += 3;

                    curr_inx[0] = south_pole;
                    curr_inx[1] = next_south_inx;
                    curr_inx[2] = curr_south_inx;
                    curr_inx += 3;
                }

                //Case when quads are to be formed
                else {
                    //Each current north/south hemisphere point , along with next points, 
                    //make quads with previous/next lattitude lines of same longitude
                    //Each longitude points are no_sides vertices spaced

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