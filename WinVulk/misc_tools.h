#pragma once
#include "device-mem-stuff.h"
#include "render-stuff.h"
#include "vectors.h"

#define member_size(struct_name, member)                             \
    sizeof(((struct_name *)0)->member)

// Currently used shader input vertex for current shader
struct VertexInput {
    Vec3 pos;
    Vec3 normal;
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
      .location = 3,
      .offset = offsetof(VertexInput, uv),
      .format = VK_FORMAT_R32G32_SFLOAT,
    }
};


// 4: final okay
//    v shader :
//      input : vec3 pos, vec3 normal, vec2 tex coord
//      push constants : mat4 model matrix
//      desc sets : mat4[2], V and P matrices
//    f shader :
//      push constants : vec3 model color
//      desc sets : (maybe later dynamic), vec3 light sources
//      + need to add depth buffer, so depth output too
//


// Currently used push constants for current shader

// Create descriptor layouts
enum CreateDescriptorLayoutsCodes {
    CREATE_DESCRIPTOR_LAYOUTS_ERR = -0x7fff,
    CREATE_DESCRIPTOR_LAYOUTS_MATRICES_LAYOUT_FAIL,
    CREATE_DESCRIPTOR_LAYOUTS_LIGHTS_LAYOUT_FAIL,
    CREATE_DESCRIPTOR_LAYOUTS_OK = 0,
};

typedef struct {
    VkDevice device;
    VkDescriptorSetLayout *p_matrix_set_layout;
    VkDescriptorSetLayout *p_lights_set_layout;

} CreateDescriptorLayoutsParam;

struct DescriptorMats {
    Mat4 view;
    Mat4 proj;
};

struct DescriptorLight {
    Vec4 light_src;
    Vec4 light_col;
};

int create_descriptor_layouts(const VkAllocationCallbacks *alloc_callbacks,
                              CreateDescriptorLayoutsParam param) {


    VkResult result = VK_SUCCESS;

    VkDescriptorSetLayoutBinding bindings[] = {
        {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,

        },
    };

    VkDescriptorSetLayoutCreateInfo set_layout_create = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pBindings = bindings,
        .bindingCount = COUNT_OF(bindings),
    };

    result = vkCreateDescriptorSetLayout(
      param.device, &set_layout_create, alloc_callbacks,
      param.p_lights_set_layout);

    if (result != VK_SUCCESS)
        return CREATE_DESCRIPTOR_LAYOUTS_LIGHTS_LAYOUT_FAIL;

    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].descriptorCount = 2;
    result = vkCreateDescriptorSetLayout(
      param.device, &set_layout_create, alloc_callbacks,
      param.p_matrix_set_layout);

    if (result != VK_SUCCESS)
        return CREATE_DESCRIPTOR_LAYOUTS_MATRICES_LAYOUT_FAIL;


    return CREATE_DESCRIPTOR_LAYOUTS_OK;
}


typedef struct {
    VkDevice device;


    VkDescriptorSetLayout *p_matrix_set_layout;
    VkDescriptorSetLayout *p_lights_set_layout;
} ClearDescriptorLayoutsParam;

void clear_descriptor_layouts(const VkAllocationCallbacks *alloc_callbacks,
                              ClearDescriptorLayoutsParam param,
                              int err_code) {
    switch (err_code) {
    case CREATE_DESCRIPTOR_LAYOUTS_OK:

        vkDestroyDescriptorSetLayout(
          param.device, *param.p_matrix_set_layout, alloc_callbacks);
    case CREATE_DESCRIPTOR_LAYOUTS_MATRICES_LAYOUT_FAIL:
        *param.p_matrix_set_layout = VK_NULL_HANDLE;


        vkDestroyDescriptorSetLayout(
          param.device, *param.p_lights_set_layout, alloc_callbacks);
    case CREATE_DESCRIPTOR_LAYOUTS_LIGHTS_LAYOUT_FAIL:
        *param.p_lights_set_layout = VK_NULL_HANDLE;


    case CREATE_DESCRIPTOR_LAYOUTS_ERR:

        break;
    }
}


enum CreateDescriptorPoolCodes {
    CREATE_DESCRIPTOR_POOL_FAIL = -0x7fff,
    CREATE_DESCRIPTOR_POOL_OK = 0
};
typedef struct {
    VkDevice device;
    size_t no_matrix_sets;
    size_t no_light_sets;

    VkDescriptorPool *p_descriptor_pool;
} CreateDescriptorPoolParam;


int create_descriptor_pool(const VkAllocationCallbacks *alloc_callbacks,
                           CreateDescriptorPoolParam param) {

    VkResult res = VK_SUCCESS;
    


    VkDescriptorPoolSize pool_sizes[] = {
        {
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = param.no_light_sets + param.no_matrix_sets * 2,
        },

        //for imgui
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount=1,
},
    };

    VkDescriptorPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = param.no_light_sets + param.no_matrix_sets + 1,
        .poolSizeCount = COUNT_OF(pool_sizes),
        .pPoolSizes = pool_sizes,
    };

    res = vkCreateDescriptorPool(param.device, &create_info,
                                 alloc_callbacks, param.p_descriptor_pool);
    if (res != VK_SUCCESS)
        return CREATE_DESCRIPTOR_POOL_FAIL;

    return CREATE_DESCRIPTOR_POOL_OK;
}


typedef struct {
    VkDevice device;

    VkDescriptorPool *p_descriptor_pool;
} ClearDescriptorPoolParam;
void clear_descriptor_pool(const VkAllocationCallbacks *alloc_callbacks,
                           ClearDescriptorPoolParam param,
                           int err_code) {
    switch (err_code) {
    case CREATE_DESCRIPTOR_POOL_OK:
        vkDestroyDescriptorPool(
          param.device, *param.p_descriptor_pool,
                                alloc_callbacks);
    case CREATE_DESCRIPTOR_POOL_FAIL:
        *param.p_descriptor_pool = VK_NULL_HANDLE;
    }
}

typedef struct {
    VkDevice device;

    VkDescriptorPool pool;


    VkDescriptorSetLayout matrix_set_layout;
    size_t matrix_sets_count;
    GpuAllocrAllocatedBuffer *p_matrix_buffers; //One buffer for each set
    VkDescriptorSet *p_matrix_sets;             //Should be preallocated

    VkDescriptorSetLayout lights_set_layout;
    size_t lights_sets_count;
    GpuAllocrAllocatedBuffer *p_light_buffers;  //One buffer for each set
    VkDescriptorSet *p_light_sets;              //Should be preallocated

} AllocateAndBindDescriptorsParam;

enum AllocateAndBindDescriptorsCodes {
    ALLOCATE_AND_BIND_DESCRIPTORS_FAIL = -0x7fff,
    ALLOCATE_AND_BIND_DESCRIPTORS_MATRIX_ALLOC_DESCRIPTORS_FAIL,
    ALLOCATE_AND_BIND_DESCRIPTORS_MATRIX_INTERNAL_ALLOC_FAIL,
    ALLOCATE_AND_BIND_DESCRIPTORS_LIGHTS_ALLOC_DESCRIPTORS_FAIL,
    ALLOCATE_AND_BIND_DESCRIPTORS_LIGHTS_INTERNAL_ALLOC_FAIL,
    ALLOCATE_AND_BIND_DESCRIPTORS_OK = 0,
};

// Allocates a bunch of sets for singular set layout
int allocate_and_bind_descriptors(
  StackAllocator *p_stk_allocr, size_t stk_offset,
  VkAllocationCallbacks *alloc_callbacks,
  AllocateAndBindDescriptorsParam param) {
    
      {
        size_t off = stk_offset;
        VkDescriptorSetLayout *dummy = stack_allocate(
          p_stk_allocr, &off,
          sizeof(VkDescriptorSetLayout) * param.lights_sets_count,
          sizeof(VkDescriptorSetLayout));

        if (dummy == NULL)
            return ALLOCATE_AND_BIND_DESCRIPTORS_LIGHTS_INTERNAL_ALLOC_FAIL;

        for (size_t i = 0; i < param.lights_sets_count; ++i)
            dummy[i] = param.lights_set_layout;

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = param.pool,
            .pSetLayouts = dummy,
            .descriptorSetCount = param.lights_sets_count,
        };

        VkResult res = vkAllocateDescriptorSets(
          param.device, &alloc_info, param.p_light_sets);
        if (res != VK_SUCCESS)
            return ALLOCATE_AND_BIND_DESCRIPTORS_LIGHTS_ALLOC_DESCRIPTORS_FAIL;
    }
      {
        size_t off = stk_offset;
        VkDescriptorSetLayout *dummy = stack_allocate(
          p_stk_allocr, &off,
          sizeof(VkDescriptorSetLayout) * param.matrix_sets_count,
          sizeof(VkDescriptorSetLayout));

        if (dummy == NULL)
            return ALLOCATE_AND_BIND_DESCRIPTORS_MATRIX_INTERNAL_ALLOC_FAIL;

        for (size_t i = 0; i < param.matrix_sets_count; ++i)
            dummy[i] = param.matrix_set_layout;

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = param.pool,
            .pSetLayouts = dummy,
            .descriptorSetCount = param.matrix_sets_count,
        };

        VkResult res = vkAllocateDescriptorSets(
          param.device, &alloc_info, param.p_matrix_sets);
        if (res != VK_SUCCESS)
            return ALLOCATE_AND_BIND_DESCRIPTORS_MATRIX_ALLOC_DESCRIPTORS_FAIL;
    }


    VkWriteDescriptorSet *writes = stack_allocate(
      p_stk_allocr, &stk_offset,
      sizeof(VkWriteDescriptorSet) *
        (param.lights_sets_count + param.matrix_sets_count),
      sizeof(uintptr_t));

    VkDescriptorBufferInfo *buff_infos = stack_allocate(
      p_stk_allocr, &stk_offset,
      sizeof(VkDescriptorBufferInfo) *
        (param.lights_sets_count + 2 * param.matrix_sets_count),
      sizeof(uintptr_t));

    if (!writes || !buff_infos)
        return ALLOCATE_AND_BIND_DESCRIPTORS_FAIL;

    int offset = 0;
    for(int i = 0; i < param.lights_sets_count; ++i) {
        writes[i + offset] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = param.p_light_sets[i],
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = buff_infos + i + offset,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,

        };
        buff_infos[i + offset] = (VkDescriptorBufferInfo){
            .buffer = param.p_light_buffers[i].buffer,
            .offset = 0,
            .range = sizeof(struct DescriptorLight),
        };
    }
    offset += param.lights_sets_count;

    for(int i = 0; i < param.matrix_sets_count; ++i) {
        writes[i + offset] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = param.p_matrix_sets[i],
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = buff_infos + 2*i + offset,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 2,

        };
        buff_infos[2*i +0 + offset] = (VkDescriptorBufferInfo){
            .buffer = param.p_matrix_buffers[i].buffer,
            .offset = offsetof(struct DescriptorMats,view),
            .range = member_size(struct DescriptorMats,view),
        };
        buff_infos[2*i +1+ offset] = (VkDescriptorBufferInfo){
            .buffer = param.p_matrix_buffers[i].buffer,
            .offset = offsetof(struct DescriptorMats,proj),
            .range = member_size(struct DescriptorMats,proj),
        };
    }


    vkUpdateDescriptorSets(param.device,
                           param.lights_sets_count + param.matrix_sets_count,
                           writes, 0, NULL);

    return ALLOCATE_AND_BIND_DESCRIPTORS_OK;
}

struct PushConst {
    struct {
        Mat4 model_mat;
    }vert_consts;
    struct {
        Vec4 model_col;
    }frag_consts;
};

typedef struct PushConst PushConst;

static const VkPushConstantRange push_ranges[] = {
    {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = offsetof(PushConst, vert_consts),
      .size = member_size(PushConst, vert_consts),
    },
    {
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = offsetof(PushConst, frag_consts),
      .size = member_size(PushConst, frag_consts),
    },
};

static const size_t push_range_count = COUNT_OF(push_ranges);

static const char *vert_file_name = "shaders/out/demo0.vert.hlsl.spv";
static const char *frag_file_name = "shaders/out/demo0.frag.hlsl.spv";

typedef uint16_t IndexInput;
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
int create_model(const VkAllocationCallbacks *alloc_callbacks,
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

void clear_model(const VkAllocationCallbacks *alloc_callbacks,
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

void submit_model_draw(const struct Model *p_model,
                       VkCommandBuffer cmd_buffer) {

    vkCmdBindVertexBuffers(cmd_buffer, 0, 1,
                           &p_model->vert_buffer.buffer,
                           (VkDeviceSize[]){ 0 });
    vkCmdBindIndexBuffer(cmd_buffer, p_model->index_buffer.buffer, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buffer, p_model->indices_count, 1, 0, 0, 0);
    // vkCmdDraw(cmd_buffer, p_model->vertex_count, 1, 0, 0);
}


struct Object3D {
    struct Model *ptr_model;
    Vec3 translate;
    Vec3 rotate;        //Angle in radians
    Vec3 scale;

    Vec3 color;
};

PushConst object_process_push_const(struct Object3D obj) {

    Mat4 translate = mat4_translate_3(obj.translate.x, obj.translate.y,
                                      obj.translate.z);
    Mat4 rotate = mat4_rotation_XYZ(obj.rotate);
    Mat4 scale = mat4_scale_3(obj.scale.x, obj.scale.y, obj.scale.z);

    PushConst pushes;
    pushes.vert_consts.model_mat =
      mat4_multiply_mat_3(&translate, &rotate, &scale);
    pushes.frag_consts.model_col = vec4_from_vec3(obj.color, 1.0f);
    return pushes;
}