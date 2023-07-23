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
}


