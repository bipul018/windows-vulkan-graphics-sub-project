#pragma once
#include "device-mem-stuff.h"
#include "render-stuff.h"
#include "vectors.h"


// Currently used push constants for current shader
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

// Currently used shader input vertex for current shader
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


//For uniforms and descriptors used in shaders currently

//Create descriptor layouts 

void test1() {

    VkDescriptorSetLayoutBinding binds[2];

    //I think this is the (set, binding)'s binding in shader
    binds[0].binding = 0;
    binds[1].binding = 1;

    //This specifies the array elements count in the shader as a array if needed
    //Else just set to 1 , maybe...
    binds[0].descriptorCount = 3;
    binds[1].descriptorCount = 3;

    //Obvs, this specifies what type of variable it will be, uniform, sampler, buffer, ... i think
    binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binds[1].descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    //This for sure specifies what shader to use in
    binds[0].stageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    binds[1].stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    

    //This is used to create (a descriptor set )->layout for pipeline layout , maybe
    VkDescriptorSetLayoutCreateInfo info;
    info.bindingCount = 2;
    info.pBindings = binds;

    VkDescriptorSetLayout des_layouts[2];

    //So, it means , i think, multiple descriptors are used , like above (binds)
    //then a descriptor set layout is created per set as above
    //Now when creating pipeline, for filling each set (set, binding), we specify
    //an array of descriptor set layout
    VkPipelineLayoutCreateInfo pipe_info;
    pipe_info.setLayoutCount = 2;
    pipe_info.pSetLayouts = des_layouts;


    //Now let's proceed to creating descriptors, and pools
    //I'll comment what roughly I will have to do 

    

    //Create Descriptor pool

    VkDescriptorPoolSize pool_sizes[2];

    //This says, acc to first pool_size, 2 uniform buffer descriptors can be created from this pool
    pool_sizes[0].descriptorCount = 2;
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    //This now says, 2 combined image samplers can be created
    pool_sizes[1].descriptorCount = 2;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;


    VkDescriptorPoolCreateInfo des_pool_info;
    des_pool_info.poolSizeCount = 2;
    des_pool_info.pPoolSizes = pool_sizes;

    //This says sum total of descriptor set count 4, afaik, that can be allocated from this pool
    des_pool_info.maxSets = 4;
    

    VkDescriptorPool des_pool;
    


    //Allocate descriptors, not sets, singular units of descriptors, 
    //I think this will use the layouts of sets, maybe, but that will mean 
    //That descriptors are created in sets anyways
    //Wait a minute,.... maybe all descriptors are created in sets, fuck

    VkDescriptorSetAllocateInfo alloc_info;
    //Now this allocates the sets as per given set layouts from the pool
    alloc_info.descriptorPool = NULL;
    alloc_info.descriptorSetCount = 2;
    alloc_info.pSetLayouts = NULL;
    
    VkDescriptorSet sets[3];
    vkAllocateDescriptorSets(NULL, &alloc_info, sets);

    //Write/update to the descriptors with buffer/images/samplers
    VkCopyDescriptorSet copy;
    //Other stuff are trivial, these mean something if descriptor is an array type
    copy.srcArrayElement = 0;   //
    copy.dstArrayElement = 1;
    copy.descriptorCount = 4;   // Total number of array descriptions to copy, copies from start of srcArrayElement, copied to from start of dst Array Element

    VkWriteDescriptorSet write;
    //Write is also the same as copy, only here there is no src
    //array also work similarly
    //For src, we have either of pImageInfo, for images, pBufferInfo for buffer, or pTexelBufferView
    //for ..., only one of these three is selected as per descriptorType




    //After all this , write to buffer memory/image memory and bind to pipeline
    //when recording/before command buffers

    vkCmdBindDescriptorSets(NULL, VK_PIPELINE_BIND_POINT_COMPUTE,
                            NULL, 1, 4, sets, 0, NULL);
    //Trivial, binds descriptor sets in sets, from 0 to 3, (4 sets)
    //to descriptor slots 1 to 4 (4 set slots) for compute pipeline
    //The last two members are special, they are used if dynamic arrays are expected in any of slots
    //i.e, array size in shader is unkonwn at shader compile time
    //Probably not hard at all, if needed be

}

// Now let's make a game plan, like how many and how types of descriptor sets
// to create 
// 
// 0: v shader : 
//      0,0 : float4x4[] : each one successively being V and P matrices
//    f shader : push constant, make it simple for now
//      float4 : light pos
//    v shader : push constant, 
//      float4x4 : model matrix
// 
// 1: v shader : 
//      0,0 : float4x4[3] : each one successively being M V and P matrices
//    f shader :
//      0,1 : light pos
// 
// 2: v shader
//      0,0 : float4x4 : model matrix
//      0,1 : float4   : model color, override the vertex color here
//      1,0 : float4x4[2] : V and P matrix
//      1,1 : float3 : light position
//      
//






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