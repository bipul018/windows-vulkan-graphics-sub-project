#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "common-stuff.h"
#include "device-mem-stuff.h"
#include "render-stuff.h"
#include "window-stuff.h"

struct WinProcData {
    UINT_PTR timer_id;
    int width;
    int height;
    HANDLE win_handle;
};


void *VKAPI_PTR cb_allocation_fxn(void *user_data, size_t size,
                                  size_t align,
                                  VkSystemAllocationScope scope) {

    size_t *ptr = user_data;
    (*ptr) += size;
    printf("%zu : %zu Alloc\t", *ptr, align);
    return _aligned_offset_malloc(size, 8, 0);
}

void *VKAPI_PTR cb_reallocation_fxn(void *user_data, void *p_original,
                                    size_t size, size_t align,
                                    VkSystemAllocationScope scope) {
    size_t *ptr = user_data;
    printf("%zu : %zu Realloc\t", *ptr, align);
    return _aligned_offset_realloc(p_original, size, 8, 0);
}
#include <malloc.h>

void VKAPI_PTR cb_free_fxn(void *user_data, void *p_mem) {
    size_t *ptr = user_data;
    //(*ptr) -= _aligned_msize(p_mem, 8, 0);
    printf("%zu Free\t", *ptr);
    _aligned_free(p_mem);
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

    VkClearValue img_clear_col = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

    VkRenderPassBeginInfo rndr_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = param.render_pass,
        .framebuffer = param.framebuffers[*param.p_img_inx],
        .renderArea = { .offset = { 0, 0 },
                        .extent = param.framebuffer_render_extent },
        .clearValueCount = 1,
        .pClearValues = &img_clear_col,

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
    if (vkQueueSubmit(param.graphics_queue, 1, &render_submit_info,
                      param.render_done_fence) != VK_SUCCESS)
        return END_RENDERING_OPERATIONS_GRAPHICS_QUEUE_SUBMIT_FAIL;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &param.swapchain,
        .pImageIndices = &param.img_index,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &param.render_done_semaphore,
    };

    VkResult result =
      vkQueuePresentKHR(param.present_queue, &present_info);

    if (result == VK_SUBOPTIMAL_KHR ||
        result == VK_ERROR_OUT_OF_DATE_KHR)
        return END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN;
    if (result < VK_SUCCESS)
        return END_RENDERING_OPERATIONS_FAILED;
    return END_RENDERING_OPERATIONS_OK;
}

LRESULT CALLBACK wnd_proc(HWND h_wnd, UINT msg, WPARAM wparam,
                          LPARAM lparam) {

    struct WinProcData *p_win = NULL;

    if (msg == WM_CREATE) {
        CREATESTRUCT *cr_data = (CREATESTRUCT *)lparam;
        p_win = (struct WinProcData *)cr_data->lpCreateParams;
        SetWindowLongPtr(h_wnd, GWLP_USERDATA, (LONG_PTR)p_win);

        p_win->win_handle = h_wnd;

        RECT client_area;
        GetClientRect(h_wnd, &client_area);
        p_win->width = client_area.right;
        p_win->height = client_area.bottom;

        p_win->timer_id = SetTimer(h_wnd, 111, 20, NULL);

    } else {
        p_win = (struct WinProcData *)GetWindowLongPtr(h_wnd,
                                                       GWLP_USERDATA);
    }

    if (p_win) {

        if (msg == WM_DESTROY) {
            KillTimer(h_wnd, p_win->timer_id);
            PostQuitMessage(0);
            return 0;
        }
        if (msg == WM_PAINT) {
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h_wnd, &ps);

            //Rectangle(hdc, 10, 10, 300, 300);

            EndPaint(h_wnd, &ps);

        }
        if (msg == WM_TIMER) {
            if (wparam == p_win->timer_id) {
                InvalidateRect(h_wnd, NULL, TRUE);
            }
        }
        if (msg == WM_SIZE) {

            RECT client_area;
            GetClientRect(h_wnd, &client_area);
            p_win->width = client_area.right;
            p_win->height = client_area.bottom;
            // recreate_swapchain(p_win);
        }
        if (msg == WM_SETCURSOR) {
            HCURSOR cursor = LoadCursor(NULL, IDC_UPARROW);
            SetCursor(cursor);
        }
    }

    return DefWindowProc(h_wnd, msg, wparam, lparam);
}

#pragma warning(push, 3)
#pragma warning(default : 4703 4700)
int main(int argc, char *argv[]) {

    enum MainFailCodes {
        MAIN_FAIL_ALL = -0x7fff,

        MAIN_FAIL_GRAPHICS_PIPELINE,
        MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT,

        MAIN_FAIL_ALLOC_MAJOR_VERTEX_BUFFER,
        MAIN_FAIL_GPU_ALLOCR,
        MAIN_FAIL_MAJOR_VERTEX_BUFFER,

        MAIN_FAIL_COMMAND_BUFFERS,
        MAIN_FAIL_COMMAND_POOL,
        MAIN_FAIL_FRAME_SEMAPHORES,
        MAIN_FAIL_FRAME_FENCES,
        MAIN_FAIL_FRAMEBUFFERS,
        MAIN_FAIL_SWAPCHAIN,
        MAIN_FAIL_RENDER_PASS,
        MAIN_FAIL_DEVICE,
        MAIN_FAIL_SURFACE,
        MAIN_FAIL_INSTANCE,
        MAIN_FAIL_BUMP_ALLOCATOR,
        MAIN_FAIL_WINDOW,
        MAIN_FAIL_OK = 0,
    } failure = MAIN_FAIL_OK;

#define return_main_fail(fail_code)                                  \
    {                                                                \
        DebugBreak();                                                \
        failure = fail_code;                                         \
        goto cleanup_phase;                                          \
    }


    HINSTANCE h_instance = GetModuleHandle(NULL);
    LPWSTR wndclass_name = L"Window Class";

    WNDCLASS wnd_class = { 0 };
    wnd_class.hInstance = h_instance;
    wnd_class.lpszClassName = wndclass_name;
    wnd_class.lpfnWndProc = wnd_proc;
    RegisterClass(&wnd_class);

    size_t allocation_amt = 0;
    VkAllocationCallbacks alloc_callbacks = {
        .pfnAllocation = cb_allocation_fxn,
        .pfnReallocation = cb_reallocation_fxn,
        .pfnFree = cb_free_fxn,
        .pUserData = &allocation_amt,
    };
    VkAllocationCallbacks *ptr_alloc_callbacks = &alloc_callbacks;

    ptr_alloc_callbacks = NULL;

    struct WinProcData winproc_data = { 0 };

    if (!CreateWindowEx(0, wndclass_name, L"Window",
                        WS_OVERLAPPEDWINDOW, 20, 10, 600, 600, NULL,
                        NULL, h_instance, &winproc_data))
        return_main_fail(MAIN_FAIL_WINDOW);

    ShowWindow(winproc_data.win_handle, SW_SHOW);

    int err_code = 0;

    // Create stack allocator
    StackAllocator bump_allocr =
      alloc_stack_allocator(SIZE_GB(2), SIZE_MB(100));
    StackAllocator *ptr_bump_allocr = &bump_allocr;
    size_t bump_offset = 0;
    if (bump_allocr.reserved_memory == 0)
        return_main_fail(MAIN_FAIL_BUMP_ALLOCATOR);

    VkInstance vk_instance = VK_NULL_HANDLE;
    // Create instance
    {
        struct VulkanLayer inst_layers[] = {
#ifndef NDEBUG
            {
              .layer_name = "VK_LAYER_KHRONOS_validation",
              .required = 1,
            },
#endif
            { 0 }
        };

        struct VulkanExtension inst_exts[] = {
#ifndef NDEBUG
            {
              .extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
              .required = 1,
            },

#endif
            {
              .extension_name = VK_KHR_SURFACE_EXTENSION_NAME,
              .required = 1,
            },
            {
              .extension_name = VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
              .required = 1,
            },

            { 0 }
        };

        err_code = create_instance(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          &vk_instance, inst_layers, COUNT_OF(inst_layers) - 1,
          inst_exts, COUNT_OF(inst_exts) - 1);

        if (err_code < 0)
            return_main_fail(MAIN_FAIL_INSTANCE);
    }

    // Create surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    {
        VkWin32SurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = h_instance,
            .hwnd = winproc_data.win_handle,
        };

        if (vkCreateWin32SurfaceKHR(vk_instance, &create_info,
                                    ptr_alloc_callbacks,
                                    &surface) != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_SURFACE);
    }

    // Create Device
    struct VulkanDevice device = { 0 };
    {
        CreateDeviceParam param = {
            .vk_instance = vk_instance,
            .chosen_surface = surface,
            .p_vk_device = &device,
        };

        struct VulkanLayer dev_layers[] = {
#ifndef NDEBUG
            {
              .layer_name = "VK_LAYER_KHRONOS_validation",
              .required = 1,
            },
#endif
            { 0 }
        };

        struct VulkanExtension dev_extensions[] = {
            { .extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME,
              .required = 1 },
            { .extension_name =
                VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
              .required = 1 },
            { 0 }
        };

        err_code = create_device(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks, param,
          dev_layers, COUNT_OF(dev_layers) - 1, dev_extensions,
          COUNT_OF(dev_extensions) - 1);

        if (err_code < 0)
            return_main_fail(MAIN_FAIL_DEVICE);
    }

    // Create Render Pass
    VkRenderPass render_pass = VK_NULL_HANDLE;
    {
        CreateRenderPassParam param = {
            .device = device.device,
            .img_format = device.img_format.format,
            .p_render_pass = &render_pass,
        };
        err_code = create_render_pass(ptr_bump_allocr, bump_offset,
                                      ptr_alloc_callbacks, param);
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_RENDER_PASS);
    }


    // Create swapchain
    struct SwapchainEntities curr_swapchain_data = { 0 };
    struct SwapchainEntities old_swapchain_data = { 0 };
    VkExtent2D swapchain_extent;
    struct SwapchainCreationInfo swapchain_creation_infos = {
        .phy_device = device.phy_device,
        .surface = surface,

        .graphics_family_inx = device.graphics_family_inx,
        .present_family_inx = device.present_family_inx,

        .surface_format = device.img_format,
        .img_pre_transform = device.transform_flags,
        .min_image_count = device.min_img_count,
        .present_mode = device.present_mode,
    };
    {
        err_code = create_swapchain(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreateSwapchainParam){
            .create_info = swapchain_creation_infos,
            .device = device.device,
            .win_height = winproc_data.height,
            .win_width = winproc_data.width,
            .old_swapchain = old_swapchain_data.swapchain,
            .curr_swapchain_data = &curr_swapchain_data,
            .p_img_swap_extent = &swapchain_extent,
          });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_SWAPCHAIN);
    }

    // Create framebuffers on swapchain
    {
        err_code = create_framebuffers(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreateFramebuffersParam){
            .device = device.device,
            .compatible_render_pass = render_pass,
            .framebuffer_count = curr_swapchain_data.img_count,
            .framebuffer_extent = swapchain_extent,
            .img_views = curr_swapchain_data.img_views,
            .p_framebuffers = &curr_swapchain_data.framebuffers,
          });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_FRAMEBUFFERS);
    }


    // Determine the max frames in flight
    size_t max_frames_in_flight =
      min(curr_swapchain_data.img_count, 3);
    size_t curr_frame_in_flight = 0;

    // Create fences for per frame sync
    VkFence *frame_fences = NULL;
    {
        err_code = create_fences(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreateFencesParam){ .device = device.device,
                               .fences_count = max_frames_in_flight,
                               .p_fences = &frame_fences });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_FRAME_FENCES);
    }

    // Create image available semaphores, and render finished
    // semaphores
    VkSemaphore *all_semaphores = NULL;
    VkSemaphore *render_finished_semaphores = NULL;
    VkSemaphore *present_done_semaphores = NULL;
    {
        err_code = create_semaphores(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreateSemaphoresParam){ .device = device.device,
                                   .semaphores_count =
                                     2 * max_frames_in_flight,
                                   .p_semaphores = &all_semaphores });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_FRAME_SEMAPHORES);
    }
    present_done_semaphores = all_semaphores;
    render_finished_semaphores =
      all_semaphores + max_frames_in_flight;


    // Create command pool
    VkCommandPool rndr_cmd_pool = VK_NULL_HANDLE;
    {
        err_code = create_command_pool(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreateCommandPoolParam){ .device = device.device,
                                    .queue_family_inx =
                                      device.graphics_family_inx,
                                    .p_cmd_pool = &rndr_cmd_pool });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_COMMAND_POOL);
    }

    // Alloc command buffers for each frame
    VkCommandBuffer *rndr_cmd_buffers = NULL;
    {
        err_code = create_primary_command_buffers(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreatePrimaryCommandBuffersParam){
            .device = device.device,
            .cmd_pool = rndr_cmd_pool,
            .cmd_buffer_count = max_frames_in_flight,
            .p_cmd_buffers = &rndr_cmd_buffers });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_COMMAND_BUFFERS);
    }


    // Info to collect for total memory
    uint32_t memory_type_flags = -1;
    size_t total_gpu_memory = 0;
    // Now create the vertex buffers, that are constant across frames
    GpuAllocrAllocatedBuffer major_vertex_buffer = { 0 };
    const float major_vertex_sample_data[][2] = {
        { -0.5f, -0.5f }, { 0.5f, 0.5f },   { -0.5f, 0.5f },
        { 0.5f , 0.5f },   { -0.5f, -0.5f }, { 0.5f, -0.5f }
    };
    {
        if (vkCreateBuffer(
              device.device,
              &(VkBufferCreateInfo){
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .size = sizeof(major_vertex_sample_data) },
              ptr_alloc_callbacks,
              &major_vertex_buffer.buffer) != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_MAJOR_VERTEX_BUFFER);

        // Query all required memory properties
        VkMemoryRequirements buffer_reqs;
        vkGetBufferMemoryRequirements(
          device.device, major_vertex_buffer.buffer, &buffer_reqs);
        major_vertex_buffer.base_align = buffer_reqs.alignment;
        major_vertex_buffer.total_amt = buffer_reqs.size;
        memory_type_flags &= buffer_reqs.memoryTypeBits;
        total_gpu_memory += buffer_reqs.size;
    }


    // Now create gpu memory allocator
    GPUAllocator gpu_mem_allocr = { 0 };
    {
        err_code = allocate_device_memory(
          ptr_alloc_callbacks,
          (AllocateDeviceMemoryParam){
            .device = device.device,
            .phy_device = device.phy_device,
            .p_gpu_allocr = &gpu_mem_allocr,
            .allocation_size = total_gpu_memory,
            .memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            .memory_type_flag_bits = memory_type_flags });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_GPU_ALLOCR);
    }


    // Now start binding the vertex buffers to gpu memory, and start
    // copying data
    {
        err_code = gpu_allocr_allocate_buffer(
          &gpu_mem_allocr, device.device, &major_vertex_buffer);
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_ALLOC_MAJOR_VERTEX_BUFFER);
        if (major_vertex_buffer.mapped_memory == NULL)
            return_main_fail(MAIN_FAIL_ALLOC_MAJOR_VERTEX_BUFFER);
        memcpy(major_vertex_buffer.mapped_memory,
               major_vertex_sample_data,
               sizeof(major_vertex_sample_data));

        if (vkFlushMappedMemoryRanges(
              device.device, 1,
              &(VkMappedMemoryRange){
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = gpu_mem_allocr.memory_handle,
                .size = major_vertex_buffer.total_amt,
                .offset =
                  ((uint8_t *)major_vertex_buffer.mapped_memory -
                   (uint8_t *)gpu_mem_allocr.mapped_memory) }) !=
            VK_SUCCESS)
            return_main_fail(MAIN_FAIL_ALLOC_MAJOR_VERTEX_BUFFER);
    }


    // Now create the graphics pipeline layout, with only push
    // constant of a float for now
    VkPipelineLayout graphics_pipeline_layout = VK_NULL_HANDLE;
    {
        VkPushConstantRange push_range = {
            .offset = 0,
            .size = sizeof(float),
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        };

        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_range,


        };

        if (vkCreatePipelineLayout(
              device.device, &create_info, ptr_alloc_callbacks,
              &graphics_pipeline_layout) != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT);
    }

    // Now create graphics pipeline
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    {
        GraphicsPipelineCreationInfos create_infos =
          default_graphics_pipeline_creation_infos();

        VkVertexInputAttributeDescription attr_desc = {
            .binding = 0,
            .offset = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
        };
        VkVertexInputBindingDescription bind_desc = {
            .binding = 0,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            .stride = sizeof(major_vertex_sample_data[0]),
        };
        create_infos.vertex_input_state
          .vertexAttributeDescriptionCount = 1;
        create_infos.vertex_input_state.pVertexAttributeDescriptions =
          &attr_desc;
        create_infos.vertex_input_state
          .vertexBindingDescriptionCount = 1;
        create_infos.vertex_input_state.pVertexBindingDescriptions =
          &bind_desc;

        // create_infos.vertex_input_state
        //   .vertexAttributeDescriptionCount =
        //   create_infos.vertex_input_state
        //     .vertexBindingDescriptionCount = 0;

        err_code = create_graphics_pipeline(
          ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
          (CreateGraphicsPipelineParam){
            .create_infos = create_infos,
            .compatible_render_pass = render_pass,
            .device = device.device,
            .pipe_layout = graphics_pipeline_layout,
            .subpass_index = 0,
            .vert_shader_file = "shaders/out/demo.vert.spv",
            .frag_shader_file = "shaders/out/demo.frag.spv",
            .p_pipeline = &graphics_pipeline });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_GRAPHICS_PIPELINE);
    }

    // Now place for miscellaneous data
    int square_offset = 0;
    int max_offsets = 256;

    MSG msg = { 0 };
    while (msg.message != WM_QUIT) {
        square_offset = (square_offset + 1) % max_offsets;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        uint32_t img_inx = 0;
        BeginRenderingOperationsParam begin_param = {
            .device = device.device,
            .render_pass = render_pass,
            .framebuffer_render_extent = swapchain_extent,
            .swapchain = curr_swapchain_data.swapchain,
            .framebuffers = curr_swapchain_data.framebuffers,
            .cmd_buffer = rndr_cmd_buffers[curr_frame_in_flight],
            .render_done_fence = frame_fences[curr_frame_in_flight],
            .present_done_semaphore =
              present_done_semaphores[curr_frame_in_flight],
            .p_img_inx = &img_inx,

        };
        int res = begin_rendering_operations(begin_param);

        if (res < 0)
            continue;
        if (res ==
            BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN) {
            recreate_swapchain(
              ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
              (RecreateSwapchainParam){
                .device = device.device,
                .create_info = swapchain_creation_infos,
                .framebuffer_render_pass = render_pass,
                .new_win_height = winproc_data.height,
                .new_win_width = winproc_data.width,
                .p_img_swap_extent = &swapchain_extent,
                .p_old_swapchain_data = &old_swapchain_data,
                .p_new_swapchain_data = &curr_swapchain_data });

            continue;
        }


        // Here now set viewports, bind pipeline , set constants, bind
        // buffers and draw
        vkCmdSetViewport(
          rndr_cmd_buffers[curr_frame_in_flight], 0, 1,
          &(VkViewport){ .x = 0.f,
                         .y = 0.f,
                         .minDepth = 0.f,
                         .maxDepth = 1.f,
                         .width = swapchain_extent.width,
                         .height = swapchain_extent.height });
        vkCmdSetScissor(rndr_cmd_buffers[curr_frame_in_flight], 0, 1,
                        &(VkRect2D){ .offset = { .x = 0, .y = 0 },
                                     .extent = swapchain_extent });

        vkCmdBindPipeline(rndr_cmd_buffers[curr_frame_in_flight],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphics_pipeline);

        VkDeviceSize offsets = 0;

        vkCmdBindVertexBuffers(rndr_cmd_buffers[curr_frame_in_flight],
                               0, 1, &major_vertex_buffer.buffer,
                               &offsets);
        //                       &(VkDeviceSize){ 0 });


        vkCmdPushConstants(
          rndr_cmd_buffers[curr_frame_in_flight],
          graphics_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
          sizeof(float),
          &(float){ (float)square_offset / (max_offsets >> 1) -
                    1.f });
        ;
        vkCmdDraw(rndr_cmd_buffers[curr_frame_in_flight], 6, 1, 0, 0);


        EndRenderingOperationsParam end_param = {
            .device = device.device,
            .cmd_buffer = rndr_cmd_buffers[curr_frame_in_flight],
            .graphics_queue = device.graphics_queue,
            .present_queue = device.present_queue,
            .swapchain = curr_swapchain_data.swapchain,
            .img_index = img_inx,
            .render_done_fence = frame_fences[curr_frame_in_flight],
            .render_done_semaphore =
              render_finished_semaphores[curr_frame_in_flight],
            .present_done_semaphore =
              present_done_semaphores[curr_frame_in_flight]
        };
        res = end_rendering_operations(end_param);
        if (res ==
            END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN) {
            recreate_swapchain(
              ptr_bump_allocr, bump_offset, ptr_alloc_callbacks,
              (RecreateSwapchainParam){
                .device = device.device,
                .create_info = swapchain_creation_infos,
                .framebuffer_render_pass = render_pass,
                .new_win_height = winproc_data.height,
                .new_win_width = winproc_data.width,
                .p_img_swap_extent = &swapchain_extent,
                .p_old_swapchain_data = &old_swapchain_data,
                .p_new_swapchain_data = &curr_swapchain_data });

            continue;
        }

        if (old_swapchain_data.img_count)
            old_swapchain_data.img_count--;
        curr_frame_in_flight =
          (curr_frame_in_flight + 1) % max_frames_in_flight;
    }


    failure = MAIN_FAIL_OK;

    vkDeviceWaitIdle(device.device);

cleanup_phase:
    switch (failure) {
        // Those who take in err_code, will be below their fail
        // case label Else they will be above theier fail case
        // label
        err_code = 0;
    case MAIN_FAIL_OK:


        clear_graphics_pipeline(ptr_alloc_callbacks,
                                (ClearGraphicsPipelineParam){
                                  .device = device.device,
                                  .p_pipeline = &graphics_pipeline },
                                err_code);
        err_code = 0;
    case MAIN_FAIL_GRAPHICS_PIPELINE:

        vkDestroyPipelineLayout(device.device,
                                graphics_pipeline_layout,
                                ptr_alloc_callbacks);
        err_code = 0;
    case MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT:


        err_code = 0;
    case MAIN_FAIL_ALLOC_MAJOR_VERTEX_BUFFER:

        err_code = 0;
    case MAIN_FAIL_GPU_ALLOCR:
        free_device_memory(
          ptr_alloc_callbacks,
          (FreeDeviceMemoryParam){ .device = device.device,
                                   .p_gpu_allocr = &gpu_mem_allocr },
          err_code);


        vkDestroyBuffer(device.device, major_vertex_buffer.buffer,
                        ptr_alloc_callbacks);
        err_code = 0;
    case MAIN_FAIL_MAJOR_VERTEX_BUFFER:
        major_vertex_buffer = (GpuAllocrAllocatedBuffer){ 0 };

        err_code = 0;
    case MAIN_FAIL_COMMAND_BUFFERS:
        clear_primary_command_buffers(
          ptr_alloc_callbacks,
          (ClearPrimaryCommandBuffersParam){ .p_cmd_buffers =
                                               &rndr_cmd_buffers },
          err_code);

        err_code = 0;
    case MAIN_FAIL_COMMAND_POOL:
        clear_command_pool(
          ptr_alloc_callbacks,
          (ClearCommandPoolParam){ .device = device.device,
                                   .p_cmd_pool = &rndr_cmd_pool },
          err_code);

        err_code = 0;
    case MAIN_FAIL_FRAME_SEMAPHORES:
        clear_semaphores(
          ptr_alloc_callbacks,
          (ClearSemaphoresParam){ .device = device.device,
                                  .semaphores_count =
                                    2 * max_frames_in_flight,
                                  .p_semaphores = &all_semaphores },
          err_code);
        present_done_semaphores = NULL;
        render_finished_semaphores = NULL;

        err_code = 0;
    case MAIN_FAIL_FRAME_FENCES:
        clear_fences(
          ptr_alloc_callbacks,
          (ClearFencesParam){ .device = device.device,
                              .fences_count = max_frames_in_flight,
                              .p_fences = &frame_fences },
          err_code);


        clear_framebuffers(
          ptr_alloc_callbacks,
          (ClearFramebuffersParam){
            .device = device.device,
            .framebuffer_count = old_swapchain_data.img_count,
            .p_framebuffers = &old_swapchain_data.framebuffers },
          0);
        err_code = 0;
    case MAIN_FAIL_FRAMEBUFFERS:
        clear_framebuffers(
          ptr_alloc_callbacks,
          (ClearFramebuffersParam){
            .device = device.device,
            .framebuffer_count = curr_swapchain_data.img_count,
            .p_framebuffers = &curr_swapchain_data.framebuffers },
          err_code);


        clear_swapchain(ptr_alloc_callbacks,
                        (ClearSwapchainParam){
                          .device = device.device,
                          .p_swapchain_data = &old_swapchain_data },
                        0);
        err_code = 0;
    case MAIN_FAIL_SWAPCHAIN:
        clear_swapchain(ptr_alloc_callbacks,
                        (ClearSwapchainParam){
                          .device = device.device,
                          .p_swapchain_data = &curr_swapchain_data },
                        err_code);

        err_code = 0;
    case MAIN_FAIL_RENDER_PASS:
        clear_render_pass(
          ptr_alloc_callbacks,
          (ClearRenderPassParam){ .device = device.device,
                                  .p_render_pass = &render_pass },
          err_code);

        err_code = 0;
    case MAIN_FAIL_DEVICE:
        clear_device(
          ptr_alloc_callbacks,
          (ClearDeviceParam){ .p_device = &device.device,
                              .p_phy_device = &device.phy_device },
          err_code);
        err_code = 0;


        vkDestroySurfaceKHR(vk_instance, surface,
                            ptr_alloc_callbacks);
    case MAIN_FAIL_SURFACE:
        surface = VK_NULL_HANDLE;

        err_code = 0;
    case MAIN_FAIL_INSTANCE:
        clear_instance(ptr_alloc_callbacks, &vk_instance, err_code);


        dealloc_stack_allocator(&bump_allocr);
        err_code = 0;
    case MAIN_FAIL_BUMP_ALLOCATOR:

    case MAIN_FAIL_WINDOW:

        UnregisterClass(wnd_class.lpszClassName, h_instance);
        err_code = 0;
    case MAIN_FAIL_ALL:

        break;
    }

    return failure;

#pragma warning(pop)
#undef return_main_fail
}
