#pragma once
#include "window-stuff.h"

// Data like render passes, vertex buffers, descriptors, synchronization objects
struct RenderData {

    VkRenderPass render_pass;

    UINT curr_frame_in_flight;
    UINT max_frames_in_flight;
    VkCommandBuffer *cmd_buffer;
    VkFence *in_flight_fences;
    VkSemaphore *image_available_semaphores;
    VkSemaphore *render_finished_semaphores;

    VkBuffer vert_buff;
    VkDescriptorPool descriptor_pool;

    // Temporary members
    VkBuffer *uniform_buffer;
    VkDescriptorSet *descriptor_sets;
};
typedef struct RenderData RenderData;

// Pipeline data
struct GraphicsPipeline {

    VkPipelineLayout graphics_pipeline_layout;
    VkPipeline graphics_pipeline;
    VkDescriptorSetLayout descriptor_layout;
};
typedef struct GraphicsPipeline GraphicsPipeline;

enum CreateRenderPassCodes {
    CREATE_RENDER_PASS_FAILED = -0x7fff,

    CREATE_RENDER_PASS_OK = 0,
};

typedef struct {
    VkDevice device;
    VkFormat img_format;

    VkRenderPass *p_render_pass;
} CreateRenderPassParam;
int create_render_pass(StackAllocator *stk_allocr, size_t stk_offset,
                       VkAllocationCallbacks *alloc_callbacks,
                       CreateRenderPassParam param) {

    VkResult result = VK_SUCCESS;

    VkAttachmentDescription attachments = {
        .format = param.img_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Determines what to do to
                                               // attachment before render
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // Whether to store rendered
                                                 // things back or not
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference attachment_refs = {
        .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // Determines the shader input / output refrenced in the subpass
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_refs,
    };

    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };

    result = vkCreateRenderPass(param.device, &create_info, alloc_callbacks,
                                param.p_render_pass);

    if (result != VK_SUCCESS)
        return CREATE_RENDER_PASS_FAILED;

    return CREATE_RENDER_PASS_OK;
}

typedef struct {
    VkDevice device;

    VkRenderPass *p_render_pass;
} ClearRenderPassParam;
void clear_render_pass(VkAllocationCallbacks *alloc_callbacks, ClearRenderPassParam param,
                       int err_codes) {
    switch (err_codes) {
    case CREATE_RENDER_PASS_OK:
        vkDestroyRenderPass(param.device, *param.p_render_pass, alloc_callbacks);
    case CREATE_RENDER_PASS_FAILED:
        *param.p_render_pass = VK_NULL_HANDLE;
    }
}

uint32_t *read_spirv_in_stk_allocr(StackAllocator *stk_allocr, size_t *p_stk_offset,
                                   const char *file_name, size_t *p_file_size) {
    if (!stk_allocr || !p_stk_offset)
        return NULL;

    FILE *file = fopen(file_name, "rb");

    if (p_file_size)
        *p_file_size = 0;
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t *buffer = stack_allocate(stk_allocr, p_stk_offset, align_up_(file_size, 4),
                                      sizeof(uint32_t));
    memset(buffer, 0, align_up_(file_size, 4));
    if (!buffer)
        return NULL;

    fread(buffer, 1, file_size, file);
    file_size = align_up_(file_size, 4);
    if (p_file_size)
        *p_file_size = file_size;

    fclose(file);

    return buffer;
}

enum CreateShaderModuleFromFileCodes {
    CREATE_SHADER_MODULE_FROM_FILE_ERROR = -0x7fff,
    CREATE_SHADER_MODULE_FROM_FILE_READ_FILE_ERROR,
    CREATE_SHADER_MODULE_FROM_FILE_OK = 0,
};

typedef struct {
    VkDevice device;
    const char *shader_file_name;

    VkShaderModule *p_shader_module;
} CreateShaderModuleFromFileParam;

int create_shader_module_from_file(StackAllocator *stk_allocr, size_t stk_offset,
                                   VkAllocationCallbacks *alloc_callbacks,
                                   CreateShaderModuleFromFileParam param) {

    VkResult result = VK_SUCCESS;

    size_t code_size = 0;
    uint32_t *shader_code = read_spirv_in_stk_allocr(stk_allocr, &stk_offset,
                                                     param.shader_file_name, &code_size);
    if (!shader_code)
        return CREATE_SHADER_MODULE_FROM_FILE_READ_FILE_ERROR;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = shader_code,
        .codeSize = code_size,
    };

    result = vkCreateShaderModule(param.device, &create_info, alloc_callbacks,
                                  param.p_shader_module);

    if (result != VK_SUCCESS)
        return CREATE_SHADER_MODULE_FROM_FILE_ERROR;

    return CREATE_SHADER_MODULE_FROM_FILE_OK;
}

struct GraphicsPipelineCreationInfos {
    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkPipelineColorBlendStateCreateInfo color_blend_state;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    VkPipelineMultisampleStateCreateInfo multisample_state;
    VkPipelineRasterizationStateCreateInfo rasterization_state;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineTessellationStateCreateInfo tessellation_state;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    VkPipelineCreateFlags flags;
};
typedef struct GraphicsPipelineCreationInfos GraphicsPipelineCreationInfos;

GraphicsPipelineCreationInfos default_graphics_pipeline_creation_infos() {
    GraphicsPipelineCreationInfos infos;

    static const VkVertexInputBindingDescription default_input_bindings[] = { {
      .binding = 0,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      .stride = sizeof(float),
    } };

    static const VkVertexInputAttributeDescription default_input_attrs[] = {

        {
          .offset = 0,
          .binding = 0,
          .location = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
        }

    };

    infos.vertex_input_state = (VkPipelineVertexInputStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pVertexBindingDescriptions = default_input_bindings,
        .vertexBindingDescriptionCount = COUNT_OF(default_input_bindings),
        .pVertexAttributeDescriptions = default_input_attrs,
        .vertexAttributeDescriptionCount = COUNT_OF(default_input_attrs),
    };

    infos.input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    static const VkDynamicState default_dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    infos.dynamic_state = (VkPipelineDynamicStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = COUNT_OF(default_dynamic_states),
        .pDynamicStates = default_dynamic_states,
    };

    infos.viewport_state = (VkPipelineViewportStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    infos.rasterization_state = (VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    infos.multisample_state = (VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Place for depth/stencil testing

    static const VkPipelineColorBlendAttachmentState default_color_blend_attaches[] = {
        { .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
          .blendEnable = VK_FALSE }
    };

    infos.color_blend_state = (VkPipelineColorBlendStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .pAttachments = default_color_blend_attaches,
        .attachmentCount = COUNT_OF(default_color_blend_attaches),
    };

    return infos;
}

enum CreateGraphicsPipeline {
    CREATE_GRAPHICS_PIPELINE_FAILED = -0x7fff,

    CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED,
    CREATE_GRAPHICS_PIPELINE_OK = 0,
};

typedef struct {
    VkDevice device;
    GraphicsPipelineCreationInfos create_infos;
    VkPipelineLayout pipe_layout;
    const char *vert_shader_file;
    const char *frag_shader_file;
    const char *geom_shader_file;
    VkRenderPass compatible_render_pass;
    uint32_t subpass_index;

    VkPipeline *p_pipeline;
} CreateGraphicsPipelineParam;

int create_graphics_pipeline(StackAllocator *stk_allocr, size_t stk_offset,
                             VkAllocationCallbacks *alloc_callbacks,
                             CreateGraphicsPipelineParam param) {

    VkResult result = VK_SUCCESS;

    uint32_t shader_count = 0;

    VkShaderModule shader_modules[3] = { 0 };
    VkPipelineShaderStageCreateInfo shader_infos[3] = { 0 };

    CreateShaderModuleFromFileParam sh_param = { .device = param.device,
                                                 .p_shader_module = shader_modules };
    int shader_creation_fail = 0;
    sh_param.shader_file_name = param.vert_shader_file;
    if (create_shader_module_from_file(stk_allocr, stk_offset, alloc_callbacks,
                                       sh_param) >= 0)
        shader_creation_fail = 1 && shader_creation_fail;
    shader_infos[shader_count] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shader_modules[shader_count],
        .pName = "main",
    };

    shader_count++;
    sh_param.p_shader_module++;
    sh_param.shader_file_name = param.frag_shader_file;
    if (create_shader_module_from_file(stk_allocr, stk_offset, alloc_callbacks,
                                       sh_param) < 0)
        shader_creation_fail = 1 && shader_creation_fail;
    shader_infos[shader_count] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shader_modules[shader_count],
        .pName = "main",
    };
    shader_count++;
    sh_param.p_shader_module++;

    if (param.geom_shader_file) {
        sh_param.shader_file_name = param.geom_shader_file;
        if (create_shader_module_from_file(stk_allocr, stk_offset, alloc_callbacks,
                                           sh_param) < 0)
            shader_creation_fail = 1 && shader_creation_fail;
        shader_infos[shader_count] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
            .module = shader_modules[shader_count],
            .pName = "main",
        };
        shader_count++;
    }

    if (shader_creation_fail) {
        for (int i = 0; i < COUNT_OF(shader_modules); ++i) {
            if (shader_modules[i] != VK_NULL_HANDLE)
                vkDestroyShaderModule(param.device, shader_modules[i], alloc_callbacks);
        }
        return CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED;
    }

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shader_count,
        .pStages = shader_infos,

        .pVertexInputState = &param.create_infos.vertex_input_state,
        .pInputAssemblyState = &param.create_infos.input_assembly_state,
        .pViewportState = &param.create_infos.viewport_state,
        .pRasterizationState = &param.create_infos.rasterization_state,
        .pMultisampleState = &param.create_infos.multisample_state,
        .pDepthStencilState = &param.create_infos.depth_stencil_state,
        .pColorBlendState = &param.create_infos.color_blend_state,
        .pDynamicState = &param.create_infos.dynamic_state,

        .layout = param.pipe_layout,
        .basePipelineIndex = -1,

        .renderPass = param.compatible_render_pass,
        .subpass = param.subpass_index,

    };

    result = vkCreateGraphicsPipelines(param.device, VK_NULL_HANDLE, 1, &create_info,
                                       alloc_callbacks, param.p_pipeline);

    for (int i = 0; i < COUNT_OF(shader_modules); ++i) {
        if (shader_modules[i] != VK_NULL_HANDLE)
            vkDestroyShaderModule(param.device, shader_modules[i], alloc_callbacks);
    }

    if (result != VK_SUCCESS)
        return CREATE_GRAPHICS_PIPELINE_FAILED;

    return CREATE_GRAPHICS_PIPELINE_OK;
}

typedef struct {
    VkDevice device;

    VkPipeline *p_pipeline;
} ClearGraphicsPipelineParam;
void clear_graphics_pipeline(VkAllocationCallbacks *alloc_callbacks,
                             ClearGraphicsPipelineParam param, int err_code) {

    switch (err_code) {
    case CREATE_GRAPHICS_PIPELINE_OK:
        vkDestroyPipeline(param.device, *param.p_pipeline, alloc_callbacks);

    case CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED:

    case CREATE_GRAPHICS_PIPELINE_FAILED:

        param.p_pipeline = VK_NULL_HANDLE;

        break;
    }
}

enum CreateSemaphoresCodes {
    CREATE_SEMAPHORES_FAILED = -0x7fff,
    CREATE_SEMAPHORES_ALLOC_FAIL,
    CREATE_SEMAPHORES_OK = 0,
};

typedef struct {
    uint32_t semaphores_count;
    VkDevice device;

    VkSemaphore **p_semaphores;
} CreateSemaphoresParam;
int create_semaphores(StackAllocator *stk_allocr, size_t stk_offset,
                      VkAllocationCallbacks *alloc_callbacks,
                      CreateSemaphoresParam param) {

    VkResult result = VK_SUCCESS;

    *(param.p_semaphores) = malloc(param.semaphores_count * sizeof(VkSemaphore));

    if (!(*param.p_semaphores))
        return CREATE_SEMAPHORES_ALLOC_FAIL;

    // p_win->render_finished_semaphores = malloc(p_win->max_frames_in_flight *
    // sizeof(VkSemaphore)); res--; if (!p_win->render_finished_semaphores)
    // goto
    // clear;

    // p_win->image_available_semaphores = malloc(p_win->max_frames_in_flight *
    // sizeof(VkSemaphore)); res--; if (!p_win->image_available_semaphores)
    // goto
    // clear;

    VkSemaphoreCreateInfo sema_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    for (int i = 0; i < param.semaphores_count; ++i) {

        result = vkCreateSemaphore(param.device, &sema_create_info, alloc_callbacks,
                                   param.p_semaphores[0] + i);

        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_SEMAPHORES_FAILED;

    return CREATE_SEMAPHORES_OK;
}

typedef struct {
    VkDevice device;
    size_t semaphores_count;

    VkSemaphore **p_semaphores;
} ClearSemaphoresParam;

void clear_semaphores(VkAllocationCallbacks *alloc_callbacks, ClearSemaphoresParam param,
                      int err_codes) {

    switch (err_codes) {
    case CREATE_SEMAPHORES_OK:
        for (int i = 0; i < param.semaphores_count; ++i) {
            if (param.p_semaphores[0][i])
                vkDestroySemaphore(param.device, param.p_semaphores[0][i],
                                   alloc_callbacks);
        }

    case CREATE_SEMAPHORES_FAILED:
        free(param.p_semaphores[0]);

    case CREATE_SEMAPHORES_ALLOC_FAIL:
        *(param.p_semaphores) = NULL;
        break;
    }
}

enum CreateFencesCodes {
    CREATE_FENCES_FAILED = -0x7fff,
    CREATE_FENCES_ALLOC_FAILED,
    CREATE_FENCES_OK = 0,
};

typedef struct {
    uint32_t fences_count;
    VkDevice device;

    VkFence **p_fences;
} CreateFencesParam;
int create_fences(StackAllocator *stk_allocr, size_t stk_offset,
                  VkAllocationCallbacks *alloc_callbacks, CreateFencesParam param) {
    VkResult result = VK_SUCCESS;

    param.p_fences[0] = malloc(param.fences_count * sizeof(VkFence));

    if (!param.p_fences[0])
        return CREATE_FENCES_ALLOC_FAILED;

    // p_win->in_flight_fences = malloc(p_win->max_frames_in_flight *
    // sizeof(VkFence));

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < param.fences_count; ++i) {

        result = vkCreateFence(param.device, &fence_create_info, alloc_callbacks,
                               param.p_fences[0] + i);
        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_FENCES_FAILED;

    return CREATE_FENCES_OK;
}

typedef struct {
    VkDevice device;
    size_t fences_count;

    VkFence **p_fences;
} ClearFencesParam;

void clear_fences(VkAllocationCallbacks *alloc_callbacks, ClearFencesParam param,
                  int err_codes) {

    switch (err_codes) {
    case CREATE_FENCES_OK:
        for (int i = 0; i < param.fences_count; ++i) {
            if (param.p_fences[0][i])
                vkDestroyFence(param.device, param.p_fences[0][i], alloc_callbacks);
        }

    case CREATE_FENCES_FAILED:
        free(param.p_fences[0]);

    case CREATE_FENCES_ALLOC_FAILED:
        *(param.p_fences) = NULL;
        break;
    }
}

enum CreatePrimaryCommandBuffersCodes {
    CREATE_PRIMARY_COMMAND_BUFFERS_FAILED = -0x7fff,
    CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED,
    CREATE_PRIMARY_COMMAND_BUFFERS_OK = 0,
};

typedef struct {
    VkDevice device;
    VkCommandPool cmd_pool;
    uint32_t cmd_buffer_count;

    VkCommandBuffer **p_cmd_buffers;
} CreatePrimaryCommandBuffersParam;
int create_primary_command_buffers(StackAllocator *stk_allocr, size_t stk_offset,
                                   VkAllocationCallbacks *alloc_callbacks,
                                   CreatePrimaryCommandBuffersParam param) {

    VkResult result = VK_SUCCESS;

    param.p_cmd_buffers[0] = malloc(param.cmd_buffer_count * sizeof(VkCommandBuffer));

    if (!param.p_cmd_buffers[0])
        return CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = param.cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = param.cmd_buffer_count,
    };

    result = vkAllocateCommandBuffers(param.device, &alloc_info, param.p_cmd_buffers[0]);

    if (result != VK_SUCCESS)
        return CREATE_PRIMARY_COMMAND_BUFFERS_FAILED;

    return CREATE_PRIMARY_COMMAND_BUFFERS_OK;
}

typedef struct {

    VkCommandBuffer **p_cmd_buffers;
} ClearPrimaryCommandBuffersParam;
void clear_primary_command_buffers(VkAllocationCallbacks *alloc_callbacks,
                                   ClearPrimaryCommandBuffersParam param, int err_codes) {
    switch (err_codes) {
    case CREATE_PRIMARY_COMMAND_BUFFERS_OK:

    case CREATE_PRIMARY_COMMAND_BUFFERS_FAILED:
        free(param.p_cmd_buffers[0]);

    case CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED:
        param.p_cmd_buffers[0] = NULL;

        break;
    }
}

//
// int create_descriptor_layout(GlobalData* p_win) {
//
//	VkResult result = VK_SUCCESS;
//	int res = 0;
//
//
//	VkDescriptorSetLayoutBinding bind = {
//		.binding = 0,
//		.descriptorCount = 1,
//		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
//	};
//
//	VkDescriptorSetLayoutCreateInfo create_info = {
//		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
//		.bindingCount = 1,
//		.pBindings = &bind,
//	};
//
//
//	result = vkCreateDescriptorSetLayout(p_win->device, &create_info,
//		p_win->p_host_alloc_calls,
//		&p_win->descriptor_layout);
//	res--;
//	if (result != VK_SUCCESS)
//		return res;
//
//
//	res = 0;
//
//
//	return res;
//
//}
//
//
//
//
//
// int clear_descriptor_layout(GlobalData* p_win) {
//	vkDestroyDescriptorSetLayout(p_win->device, p_win->descriptor_layout,
//		p_win->p_host_alloc_calls);
//	return 0;
//}
