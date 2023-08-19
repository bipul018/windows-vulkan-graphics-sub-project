#pragma once
#include "window-stuff.h"

enum CreateRenderPassCodes {
    CREATE_RENDER_PASS_FAILED = -0x7fff,

    CREATE_RENDER_PASS_OK = 0,
};

typedef struct {
    VkDevice device;
    VkFormat img_format;
    VkFormat depth_stencil_format;

    VkRenderPass *p_render_pass;
} CreateRenderPassParam;
int create_render_pass(StackAllocator *stk_allocr, size_t stk_offset,
                       const VkAllocationCallbacks *alloc_callbacks,
                       CreateRenderPassParam param) {

    VkResult result = VK_SUCCESS;

    VkAttachmentDescription attachments[] = {
        { .format = param.img_format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp =
            VK_ATTACHMENT_LOAD_OP_CLEAR, // Determines what to do to
                                         // attachment before render
          .storeOp =
            VK_ATTACHMENT_STORE_OP_STORE, // Whether to store rendered
                                          // things back or not
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
        {
            .format =  param.depth_stencil_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference color_attach_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depth_attach_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    // Determines the shader input / output refrenced in the subpass
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attach_ref,
        .pDepthStencilAttachment = &depth_attach_ref,
    };

    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask =
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask =
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = COUNT_OF(attachments),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };

    result = vkCreateRenderPass(param.device, &create_info,
                                alloc_callbacks, param.p_render_pass);

    if (result != VK_SUCCESS)
        return CREATE_RENDER_PASS_FAILED;

    return CREATE_RENDER_PASS_OK;
}

typedef struct {
    VkDevice device;

    VkRenderPass *p_render_pass;
} ClearRenderPassParam;
void clear_render_pass(const VkAllocationCallbacks *alloc_callbacks,
                       ClearRenderPassParam param, int err_codes) {
    switch (err_codes) {
    case CREATE_RENDER_PASS_OK:
        vkDestroyRenderPass(param.device, *param.p_render_pass,
                            alloc_callbacks);
    case CREATE_RENDER_PASS_FAILED:
        *param.p_render_pass = VK_NULL_HANDLE;
    }
}

uint32_t *read_spirv_in_stk_allocr(StackAllocator *stk_allocr,
                                   size_t *p_stk_offset,
                                   const char *file_name,
                                   size_t *p_file_size) {
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

    uint32_t *buffer =
      stack_allocate(stk_allocr, p_stk_offset,
                     align_up_(file_size, 4), sizeof(uint32_t));
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

int create_shader_module_from_file(
  StackAllocator *stk_allocr, size_t stk_offset,
  const VkAllocationCallbacks *alloc_callbacks,
  CreateShaderModuleFromFileParam param) {

    VkResult result = VK_SUCCESS;

    size_t code_size = 0;
    uint32_t *shader_code = read_spirv_in_stk_allocr(
      stk_allocr, &stk_offset, param.shader_file_name, &code_size);
    if (!shader_code)
        return CREATE_SHADER_MODULE_FROM_FILE_READ_FILE_ERROR;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = shader_code,
        .codeSize = code_size,
    };

    result =
      vkCreateShaderModule(param.device, &create_info,
                           alloc_callbacks, param.p_shader_module);

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
typedef struct GraphicsPipelineCreationInfos
  GraphicsPipelineCreationInfos;

GraphicsPipelineCreationInfos
default_graphics_pipeline_creation_infos() {
    GraphicsPipelineCreationInfos infos;

    static const VkVertexInputBindingDescription
      default_input_bindings[] = { {
        .binding = 0,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride = sizeof(float),
      } };

    static const VkVertexInputAttributeDescription
      default_input_attrs[] = {

          {
            .offset = 0,
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
          }

      };

    infos.vertex_input_state = (VkPipelineVertexInputStateCreateInfo){
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pVertexBindingDescriptions = default_input_bindings,
        .vertexBindingDescriptionCount =
          COUNT_OF(default_input_bindings),
        .pVertexAttributeDescriptions = default_input_attrs,
        .vertexAttributeDescriptionCount =
          COUNT_OF(default_input_attrs),
    };

    infos
      .input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo){
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
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
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    infos
      .rasterization_state = (VkPipelineRasterizationStateCreateInfo){
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
    };

    infos.multisample_state = (VkPipelineMultisampleStateCreateInfo){
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Place for depth/stencil testing
    infos
      .depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo){
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    static const VkPipelineColorBlendAttachmentState
      default_color_blend_attaches[] = {
          { .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
              VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
              VK_COLOR_COMPONENT_A_BIT,
            .blendEnable = VK_FALSE }
      };

    infos.color_blend_state = (VkPipelineColorBlendStateCreateInfo){
        .sType =
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
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

int create_graphics_pipeline(StackAllocator *stk_allocr,
                             size_t stk_offset,
                             VkAllocationCallbacks *alloc_callbacks,
                             CreateGraphicsPipelineParam param) {

    VkResult result = VK_SUCCESS;

    uint32_t shader_count = 0;

    VkShaderModule shader_modules[3] = { 0 };
    VkPipelineShaderStageCreateInfo shader_infos[3] = { 0 };

    CreateShaderModuleFromFileParam sh_param = {
        .device = param.device, .p_shader_module = shader_modules
    };
    int shader_creation_fail = 0;
    sh_param.shader_file_name = param.vert_shader_file;
    if (create_shader_module_from_file(
          stk_allocr, stk_offset, alloc_callbacks, sh_param) >= 0)
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
    if (create_shader_module_from_file(stk_allocr, stk_offset,
                                       alloc_callbacks, sh_param) < 0)
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
        if (create_shader_module_from_file(
              stk_allocr, stk_offset, alloc_callbacks, sh_param) < 0)
            shader_creation_fail = 1 && shader_creation_fail;
        shader_infos[shader_count] =
          (VkPipelineShaderStageCreateInfo){
              .sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
              .module = shader_modules[shader_count],
              .pName = "main",
          };
        shader_count++;
    }

    if (shader_creation_fail) {
        for (int i = 0; i < COUNT_OF(shader_modules); ++i) {
            if (shader_modules[i] != VK_NULL_HANDLE)
                vkDestroyShaderModule(param.device, shader_modules[i],
                                      alloc_callbacks);
        }
        return CREATE_GRAPHICS_PIPELINE_SHADER_MODULES_CREATION_FAILED;
    }

    VkGraphicsPipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shader_count,
        .pStages = shader_infos,

        .pVertexInputState = &param.create_infos.vertex_input_state,
        .pInputAssemblyState =
          &param.create_infos.input_assembly_state,
        .pViewportState = &param.create_infos.viewport_state,
        .pRasterizationState =
          &param.create_infos.rasterization_state,
        .pMultisampleState = &param.create_infos.multisample_state,
        .pDepthStencilState = &param.create_infos.depth_stencil_state,
        .pColorBlendState = &param.create_infos.color_blend_state,
        .pDynamicState = &param.create_infos.dynamic_state,

        .layout = param.pipe_layout,
        .basePipelineIndex = -1,

        .renderPass = param.compatible_render_pass,
        .subpass = param.subpass_index,

    };

    result = vkCreateGraphicsPipelines(
      param.device, VK_NULL_HANDLE, 1, &create_info, alloc_callbacks,
      param.p_pipeline);

    for (int i = 0; i < COUNT_OF(shader_modules); ++i) {
        if (shader_modules[i] != VK_NULL_HANDLE)
            vkDestroyShaderModule(param.device, shader_modules[i],
                                  alloc_callbacks);
    }

    if (result != VK_SUCCESS)
        return CREATE_GRAPHICS_PIPELINE_FAILED;

    return CREATE_GRAPHICS_PIPELINE_OK;
}

typedef struct {
    VkDevice device;

    VkPipeline *p_pipeline;
} ClearGraphicsPipelineParam;
void clear_graphics_pipeline(const VkAllocationCallbacks *alloc_callbacks,
                             ClearGraphicsPipelineParam param,
                             int err_code) {

    switch (err_code) {
    case CREATE_GRAPHICS_PIPELINE_OK:
        vkDestroyPipeline(param.device, *param.p_pipeline,
                          alloc_callbacks);

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
                      const VkAllocationCallbacks *alloc_callbacks,
                      CreateSemaphoresParam param) {

    VkResult result = VK_SUCCESS;

    *(param.p_semaphores) =
      malloc(param.semaphores_count * sizeof(VkSemaphore));

    if (!(*param.p_semaphores))
        return CREATE_SEMAPHORES_ALLOC_FAIL;

    // p_win->render_finished_semaphores =
    // malloc(p_win->max_frames_in_flight * sizeof(VkSemaphore));
    // res--; if (!p_win->render_finished_semaphores) goto clear;

    // p_win->image_available_semaphores =
    // malloc(p_win->max_frames_in_flight * sizeof(VkSemaphore));
    // res--; if (!p_win->image_available_semaphores) goto clear;

    VkSemaphoreCreateInfo sema_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    for (int i = 0; i < param.semaphores_count; ++i) {

        result = vkCreateSemaphore(param.device, &sema_create_info,
                                   alloc_callbacks,
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

void clear_semaphores(const VkAllocationCallbacks *alloc_callbacks,
                      ClearSemaphoresParam param, int err_codes) {

    switch (err_codes) {
    case CREATE_SEMAPHORES_OK:
        for (int i = 0; i < param.semaphores_count; ++i) {
            if (param.p_semaphores[0][i])
                vkDestroySemaphore(param.device,
                                   param.p_semaphores[0][i],
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
                  const VkAllocationCallbacks *alloc_callbacks,
                  CreateFencesParam param) {
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

        result =
          vkCreateFence(param.device, &fence_create_info,
                        alloc_callbacks, param.p_fences[0] + i);
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

void clear_fences(const VkAllocationCallbacks *alloc_callbacks,
                  ClearFencesParam param, int err_codes) {

    switch (err_codes) {
    case CREATE_FENCES_OK:
        for (int i = 0; i < param.fences_count; ++i) {
            if (param.p_fences[0][i])
                vkDestroyFence(param.device, param.p_fences[0][i],
                               alloc_callbacks);
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
int create_primary_command_buffers(
  StackAllocator *stk_allocr, size_t stk_offset,
  VkAllocationCallbacks *alloc_callbacks,
  CreatePrimaryCommandBuffersParam param) {

    VkResult result = VK_SUCCESS;

    param.p_cmd_buffers[0] =
      malloc(param.cmd_buffer_count * sizeof(VkCommandBuffer));

    if (!param.p_cmd_buffers[0])
        return CREATE_PRIMARY_COMMAND_BUFFERS_ALLOC_FAILED;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = param.cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = param.cmd_buffer_count,
    };

    result = vkAllocateCommandBuffers(param.device, &alloc_info,
                                      param.p_cmd_buffers[0]);

    if (result != VK_SUCCESS)
        return CREATE_PRIMARY_COMMAND_BUFFERS_FAILED;

    return CREATE_PRIMARY_COMMAND_BUFFERS_OK;
}

typedef struct {

    VkCommandBuffer **p_cmd_buffers;
} ClearPrimaryCommandBuffersParam;
void clear_primary_command_buffers(
  VkAllocationCallbacks *alloc_callbacks,
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


enum BeginRenderingOperationsCodes {
    BEGIN_RENDERING_OPERATIONS_FAILED = -0x7fff,
    BEGIN_RENDERING_OPERATIONS_BEGIN_CMD_BUFFER_FAIL,
    BEGIN_RENDERING_OPERATIONS_WAIT_FOR_FENCE_FAIL,
    BEGIN_RENDERING_OPERATIONS_OK = 0,
    BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN,
};

typedef struct {
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkRenderPass render_pass;
    VkFramebuffer *framebuffers;
    VkExtent2D framebuffer_render_extent;
    VkCommandBuffer cmd_buffer;
    VkSemaphore present_done_semaphore;
    VkFence render_done_fence;
    VkClearValue clear_value;

    uint32_t *p_img_inx;

} BeginRenderingOperationsParam;
int begin_rendering_operations(BeginRenderingOperationsParam param) {
    VkResult result = VK_SUCCESS;

    result = vkWaitForFences(
      param.device, 1, &param.render_done_fence, VK_TRUE, UINT64_MAX);

    if (result != VK_SUCCESS)
        return BEGIN_RENDERING_OPERATIONS_WAIT_FOR_FENCE_FAIL;

    result = vkAcquireNextImageKHR(
      param.device, param.swapchain, UINT64_MAX,
      param.present_done_semaphore, VK_NULL_HANDLE, param.p_img_inx);

    if (result == VK_ERROR_OUT_OF_DATE_KHR ||
        !param.framebuffer_render_extent.width ||
        !param.framebuffer_render_extent.height) {
        return BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN;
    }


    vkResetCommandBuffer(param.cmd_buffer, 0);


    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,

    };

    result = vkBeginCommandBuffer(param.cmd_buffer, &cmd_begin_info);

    if (result != VK_SUCCESS)
        return BEGIN_RENDERING_OPERATIONS_BEGIN_CMD_BUFFER_FAIL;

    VkClearValue clear_values[] = {
        param.clear_value,
        (VkClearValue){
          .depthStencil = { .depth = 1.f, .stencil = 0 },
        }
    };
    
    VkRenderPassBeginInfo rndr_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = param.render_pass,
        .framebuffer = param.framebuffers[*param.p_img_inx],
        .renderArea = { .offset = { 0, 0 },
                        .extent = param.framebuffer_render_extent },
        .clearValueCount = COUNT_OF(clear_values),
        .pClearValues = clear_values,

    };
    vkCmdBeginRenderPass(param.cmd_buffer, &rndr_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    return BEGIN_RENDERING_OPERATIONS_OK;
}

enum EndRenderingOperationsCodes {
    END_RENDERING_OPERATIONS_FAILED = -0x7fff,
    END_RENDERING_OPERATIONS_GRAPHICS_QUEUE_SUBMIT_FAIL,
    END_RENDERING_OPERATIONS_END_CMD_BUFFER_FAIL,
    END_RENDERING_OPERATIONS_OK = 0,
    END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN,
};

typedef struct {
    VkDevice device;
    VkCommandBuffer cmd_buffer;

    VkSemaphore render_done_semaphore;
    VkSemaphore present_done_semaphore;
    VkFence render_done_fence;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSwapchainKHR swapchain;
    uint32_t img_index;

} EndRenderingOperationsParam;
int end_rendering_operations(EndRenderingOperationsParam param) {
    vkCmdEndRenderPass(param.cmd_buffer);
    if (vkEndCommandBuffer(param.cmd_buffer) != VK_SUCCESS)
        return END_RENDERING_OPERATIONS_END_CMD_BUFFER_FAIL;

    vkResetFences(param.device, 1, &param.render_done_fence);

    VkSubmitInfo render_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &param.cmd_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &param.render_done_semaphore,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &param.present_done_semaphore,
        .pWaitDstStageMask =
          &(VkPipelineStageFlags){
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
    };
    VkResult result = VK_SUCCESS;
    result = vkQueueSubmit(param.graphics_queue, 1, &render_submit_info,
                    param.render_done_fence);
    if (result != VK_SUCCESS)
        return END_RENDERING_OPERATIONS_GRAPHICS_QUEUE_SUBMIT_FAIL;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &param.swapchain,
        .pImageIndices = &param.img_index,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &param.render_done_semaphore,
    };

     result =
      vkQueuePresentKHR(param.present_queue, &present_info);

    if (result == VK_SUBOPTIMAL_KHR ||
        result == VK_ERROR_OUT_OF_DATE_KHR)
        return END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN;
    if (result < VK_SUCCESS)
        return END_RENDERING_OPERATIONS_FAILED;
    return END_RENDERING_OPERATIONS_OK;
}
