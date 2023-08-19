#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "common-stuff.h"
#include "device-mem-stuff.h"
#include "model_gen.h"
#include "render-stuff.h"
#include "vectors.h"
#include "window-stuff.h"


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

struct WinProcData {
    UINT_PTR timer_id;
    int width;
    int height;
    HANDLE win_handle;

    // Data that are UI controlled
    // Controlled by qweasd keys
    Vec3 *qweasd;
    // Controlled by uiojkl keys
    Vec3 *uiojkl;

    float *scroll;
};


// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                              WPARAM wParam,
                                              LPARAM lParam);

LRESULT CALLBACK wnd_proc(HWND h_wnd, UINT msg, WPARAM wparam,
                          LPARAM lparam) {

    struct WinProcData *pdata = NULL;

    if (msg == WM_CREATE) {
        CREATESTRUCT *cr_data = (CREATESTRUCT *)lparam;
        pdata = (struct WinProcData *)cr_data->lpCreateParams;
        SetWindowLongPtr(h_wnd, GWLP_USERDATA, (LONG_PTR)pdata);

        pdata->win_handle = h_wnd;

        RECT client_area;
        GetClientRect(h_wnd, &client_area);
        pdata->width = client_area.right;
        pdata->height = client_area.bottom;

        pdata->timer_id = SetTimer(h_wnd, 111, 20, NULL);

    } else {
        pdata = (struct WinProcData *)GetWindowLongPtr(h_wnd,
                                                       GWLP_USERDATA);
    }
    if (ImGui_ImplWin32_WndProcHandler(h_wnd, msg, wparam, lparam))
        return 1;
    if (pdata) {

        if (msg == WM_DESTROY) {
            KillTimer(h_wnd, pdata->timer_id);
            PostQuitMessage(0);
            return 0;
        }
        if (msg == WM_PAINT) {

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h_wnd, &ps);

            // Rectangle(hdc, 10, 10, 300, 300);

            EndPaint(h_wnd, &ps);
        }
        if (msg == WM_TIMER) {
            if (wparam == pdata->timer_id) {
                InvalidateRect(h_wnd, NULL, TRUE);
            }
        }
        if (msg == WM_SIZE) {

            RECT client_area;
            GetClientRect(h_wnd, &client_area);
            pdata->width = client_area.right;
            pdata->height = client_area.bottom;
            // recreate_swapchain(pdata);
        }
        if (msg == WM_SETCURSOR) {
            HCURSOR cursor = LoadCursor(NULL, IDC_UPARROW);
            SetCursor(cursor);
        }
        if (msg == WM_KEYDOWN) {
            switch (wparam) {
                if (pdata->qweasd) {
                case 'Q':
                    pdata->qweasd->z--;
                    break;
                case 'E':
                    pdata->qweasd->z++;
                    break;
                case 'A':
                    pdata->qweasd->x--;
                    break;
                case 'D':
                    pdata->qweasd->x++;
                    break;
                case 'W':
                    pdata->qweasd->y--;
                    break;
                case 'S':
                    pdata->qweasd->y++;
                    break;
                }
                if (pdata->uiojkl) {
                case 'U':
                    pdata->uiojkl->z--;
                    break;
                case 'O':
                    pdata->uiojkl->z++;
                    break;
                case 'J':
                    pdata->uiojkl->x--;
                    break;
                case 'L':
                    pdata->uiojkl->x++;
                    break;
                case 'I':
                    pdata->uiojkl->y--;
                    break;
                case 'K':
                    pdata->uiojkl->y++;
                    break;
                }
            }
        }
        if (msg == WM_MOUSEWHEEL) {
            if (pdata->scroll) {
                pdata->scroll[0] +=
                  (float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            }
        }
    }

    return DefWindowProc(h_wnd, msg, wparam, lparam);
}

#include "misc_tools.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "imgui/cimgui.h"

#include "imgui/imgui/imgui_impl_vulkan.h"
#include "imgui/imgui/imgui_impl_win32.h"

#pragma warning(push, 3)
#pragma warning(default : 4703 4700)
int main(int argc, char *argv[]) {

    enum MainFailCodes {
        MAIN_FAIL_ALL = -0x7fff,

        MAIN_FAIL_MODEL_LOAD,

        MAIN_FAIL_IMGUI_INIT,

        MAIN_FAIL_GRAPHICS_PIPELINE,
        MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT,

        MAIN_FAIL_DESCRIPTOR_ALLOC_AND_BIND,
        MAIN_FAIL_DESCRIPTOR_MEM_ALLOC,
        MAIN_FAIL_DESCRIPTOR_BUFFER,
        MAIN_FAIL_DESCRIPTOR_BUFFER_MEM_ALLOC,
        MAIN_FAIL_DESCRIPTOR_POOL,
        MAIN_FAIL_DESCRIPTOR_LAYOUT,


        MAIN_FAIL_GPU_ALLOCR,

        MAIN_FAIL_COMMAND_BUFFERS,
        MAIN_FAIL_COMMAND_POOL,
        MAIN_FAIL_FRAME_SEMAPHORES,
        MAIN_FAIL_FRAME_FENCES,
        MAIN_FAIL_FRAMEBUFFERS,
        MAIN_FAIL_DEPTHBUFFERS,
        MAIN_FAIL_SWAPCHAIN,
        MAIN_FAIL_RENDER_PASS,
        MAIN_FAIL_DEVICE,
        MAIN_FAIL_SURFACE,
        MAIN_FAIL_INSTANCE,
        MAIN_FAIL_STACK_ALLOCATOR,
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
                        WS_OVERLAPPEDWINDOW, 20, 10, 800, 700, NULL,
                        NULL, h_instance, &winproc_data))
        return_main_fail(MAIN_FAIL_WINDOW);


    int err_code = 0;

    // Create stack allocator
    StackAllocator stk_allocr =
      alloc_stack_allocator(SIZE_GB(2), SIZE_MB(100));
    StackAllocator *ptr_stk_allocr = &stk_allocr;
    size_t stk_offset = 0;
    if (stk_allocr.reserved_memory == 0)
        return_main_fail(MAIN_FAIL_STACK_ALLOCATOR);

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
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
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
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks, param,
          dev_layers, COUNT_OF(dev_layers) - 1, dev_extensions,
          COUNT_OF(dev_extensions) - 1);

        if (err_code < 0)
            return_main_fail(MAIN_FAIL_DEVICE);

        // For now hardcode the depth stencil format
        device.depth_stencil_format = VK_FORMAT_D32_SFLOAT;
    }

    // Create Render Pass
    VkRenderPass render_pass = VK_NULL_HANDLE;
    {
        CreateRenderPassParam param = {
            .device = device.device,
            .img_format = device.img_format.format,
            .depth_stencil_format = device.depth_stencil_format,
            .p_render_pass = &render_pass,
        };
        err_code = create_render_pass(ptr_stk_allocr, stk_offset,
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

        .depth_stencil_format = device.depth_stencil_format,
        .surface_format = device.img_format,
        .img_pre_transform = device.transform_flags,
        .min_image_count = device.min_img_count,
        .present_mode = device.present_mode,
    };
    {
        err_code = create_swapchain(
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
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

    // Create depth image and image views
    {
        err_code = create_depthbuffers(
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
          (CreateDepthbuffersParam){
            .device = device.device,
            .phy_device = device.phy_device,
            .depth_count = curr_swapchain_data.img_count,
            .img_extent = swapchain_extent,
            .depth_img_format = device.depth_stencil_format,
            .p_depth_buffers = &curr_swapchain_data.depth_imgs,
            .p_depth_buffer_views = &curr_swapchain_data.depth_views,
            .p_depth_memory = &curr_swapchain_data.device_mem,
          });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_DEPTHBUFFERS);
    }

    // Create framebuffers on swapchain
    {
        err_code = create_framebuffers(
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
          (CreateFramebuffersParam){
            .device = device.device,
            .compatible_render_pass = render_pass,
            .framebuffer_count = curr_swapchain_data.img_count,
            .framebuffer_extent = swapchain_extent,
            .img_views = curr_swapchain_data.img_views,
            .depth_views = curr_swapchain_data.depth_views,
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
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
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
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
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
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
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
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
          (CreatePrimaryCommandBuffersParam){
            .device = device.device,
            .cmd_pool = rndr_cmd_pool,
            .cmd_buffer_count = max_frames_in_flight,
            .p_cmd_buffers = &rndr_cmd_buffers });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_COMMAND_BUFFERS);
    }


    // Info to collect for total memory

    // For now , required memory types will be hardcoded, later must
    // be properly evaluated, maybe after textures are implemented
    uint32_t memory_type_flags = 255;
    size_t total_gpu_memory = SIZE_MB(50);
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

    // Create descriptor layouts
    VkDescriptorSetLayout g_matrix_layout;
    VkDescriptorSetLayout g_lights_layout;
    {
        err_code = create_descriptor_layouts(
          ptr_alloc_callbacks,
          (CreateDescriptorLayoutsParam){
            .device = device.device,
            .p_lights_set_layout = &g_lights_layout,
            .p_matrix_set_layout = &g_matrix_layout });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_DESCRIPTOR_LAYOUT);
    }

    // Create descriptor pool
    VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;
    {
        err_code = create_descriptor_pool(
          ptr_alloc_callbacks,
          (CreateDescriptorPoolParam){
            .device = device.device,
            .no_matrix_sets = max_frames_in_flight,
            .no_light_sets = max_frames_in_flight,
            .p_descriptor_pool = &g_descriptor_pool });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_DESCRIPTOR_POOL);
    }

    // Create buffers for descriptors
    GpuAllocrAllocatedBuffer *g_matrix_uniform_buffers = NULL;
    GpuAllocrAllocatedBuffer *g_lights_uniform_buffers = NULL;
    {
        GpuAllocrAllocatedBuffer *p_buffs = stack_allocate(
          ptr_stk_allocr, &stk_offset,
          sizeof(GpuAllocrAllocatedBuffer) * 2 * max_frames_in_flight,
          1);
        if (!p_buffs)
            return_main_fail(MAIN_FAIL_DESCRIPTOR_BUFFER_MEM_ALLOC);
        ZeroMemory(p_buffs,
                   2 * max_frames_in_flight * sizeof *p_buffs);
        g_matrix_uniform_buffers = p_buffs;
        g_lights_uniform_buffers = p_buffs + max_frames_in_flight;
    }
    {
        for (size_t i = 0; i < max_frames_in_flight; ++i) {
            err_code = create_and_allocate_buffer(
              ptr_alloc_callbacks, &gpu_mem_allocr, device.device,
              (CreateAndAllocateBufferParam){
                .p_buffer = g_matrix_uniform_buffers + i,
                .share_mode = VK_SHARING_MODE_EXCLUSIVE,
                .size = sizeof(struct DescriptorMats),
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              });
            if (err_code < 0)
                return_main_fail(MAIN_FAIL_DESCRIPTOR_BUFFER);
            err_code = create_and_allocate_buffer(
              ptr_alloc_callbacks, &gpu_mem_allocr, device.device,
              (CreateAndAllocateBufferParam){
                .p_buffer = g_lights_uniform_buffers + i,
                .share_mode = VK_SHARING_MODE_EXCLUSIVE,
                .size = sizeof(struct DescriptorLight),
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              });
            if (err_code < 0)
                return_main_fail(MAIN_FAIL_DESCRIPTOR_BUFFER);
        }
    }
    VkDescriptorSet *g_matrix_descriptor_sets = NULL;
    VkDescriptorSet *g_lights_descriptor_sets = NULL;
    {
        VkDescriptorSet *ptr = stack_allocate(
          ptr_stk_allocr, &stk_offset,
          sizeof(VkDescriptorSet) * max_frames_in_flight * 2,
          sizeof(uintptr_t));
        if (!ptr)
            return_main_fail(MAIN_FAIL_DESCRIPTOR_MEM_ALLOC);
        ZeroMemory(ptr, 2 * max_frames_in_flight * sizeof *ptr);

        g_matrix_descriptor_sets = ptr;
        g_lights_descriptor_sets = ptr + max_frames_in_flight;
    }
    {


        err_code = allocate_and_bind_descriptors(
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
          (AllocateAndBindDescriptorsParam){
            .device = device.device,
            .pool = g_descriptor_pool,
            .lights_sets_count = max_frames_in_flight,
            .lights_set_layout = g_lights_layout,
            .p_light_buffers = g_lights_uniform_buffers,
            .p_light_sets = g_lights_descriptor_sets,
            .matrix_sets_count = max_frames_in_flight,
            .matrix_set_layout = g_matrix_layout,
            .p_matrix_buffers = g_matrix_uniform_buffers,
            .p_matrix_sets = g_matrix_descriptor_sets,
          });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_DESCRIPTOR_ALLOC_AND_BIND);
    }

    // Now create the graphics pipeline layout, with only push
    // constant of a float for now
    VkPipelineLayout graphics_pipeline_layout = VK_NULL_HANDLE;
    {

        VkDescriptorSetLayout des_layouts[] = {
            g_matrix_layout,
            g_lights_layout,
        };

        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = push_range_count,
            .pPushConstantRanges = push_ranges,
            .setLayoutCount = COUNT_OF(des_layouts),
            .pSetLayouts = des_layouts,

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

        create_infos.vertex_input_state
          .vertexAttributeDescriptionCount = COUNT_OF(attrib_descs),
          create_infos.vertex_input_state
            .pVertexAttributeDescriptions = attrib_descs;
        create_infos.vertex_input_state
          .vertexBindingDescriptionCount = COUNT_OF(binding_descs);
        create_infos.vertex_input_state.pVertexBindingDescriptions =
          binding_descs;
        create_infos.rasterization_state.polygonMode =
          VK_POLYGON_MODE_FILL;
        /*
          VK_POLYGON_MODE_LINE;
          VK_POLYGON_MODE_POINT;
        */
        create_infos.rasterization_state.cullMode =
          VK_CULL_MODE_BACK_BIT;
        /*
        create_infos.rasterization_state.cullMode = VK_CULL_MODE_NONE;
        */
        err_code = create_graphics_pipeline(
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
          (CreateGraphicsPipelineParam){
            .create_infos = create_infos,
            .compatible_render_pass = render_pass,
            .device = device.device,
            .pipe_layout = graphics_pipeline_layout,
            .subpass_index = 0,
            .vert_shader_file = vert_file_name,
            .frag_shader_file = frag_file_name,
            .p_pipeline = &graphics_pipeline });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_GRAPHICS_PIPELINE);
    }

    ShowWindow(winproc_data.win_handle, SW_SHOW);

    // Setup imgui
    ImGui_ImplVulkan_InitInfo imgui_init_info;
    {
        igCreateContext(NULL);
        // set docking
        ImGuiIO *ioptr = igGetIO();
        ioptr->ConfigFlags |=
          ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard
                                              // Controls
        // ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
        // Enable Gamepad Controls

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(winproc_data.win_handle);

        imgui_init_info = (ImGui_ImplVulkan_InitInfo){
            .Instance = vk_instance,
            .PhysicalDevice = device.phy_device,
            .Device = device.device,
            .QueueFamily = device.graphics_family_inx,
            .Queue = device.graphics_queue,
            .PipelineCache = VK_NULL_HANDLE,
            .DescriptorPool = g_descriptor_pool,
            .Subpass = 0,
            .MinImageCount = swapchain_creation_infos.min_image_count,
            .ImageCount = curr_swapchain_data.img_count,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .Allocator = ptr_alloc_callbacks,
            .CheckVkResultFn = NULL,
        };
        ImGui_ImplVulkan_Init(&imgui_init_info, render_pass);

        igStyleColorsDark(NULL);

        // Upload Fonts
        // Use any command queue
        VkCommandPool command_pool = rndr_cmd_pool;
        VkCommandBuffer command_buffer =
          rndr_cmd_buffers[curr_frame_in_flight];
        VkResult err;
        err = vkResetCommandPool(device.device, command_pool, 0);

        if (err != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_IMGUI_INIT);

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        err = vkBeginCommandBuffer(command_buffer, &begin_info);

        if (err != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_IMGUI_INIT);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
        };
        err = vkEndCommandBuffer(command_buffer);

        if (err != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_IMGUI_INIT);

        err = vkQueueSubmit(device.graphics_queue, 1, &end_info,
                            VK_NULL_HANDLE);

        if (err != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_IMGUI_INIT);

        err = vkDeviceWaitIdle(device.device);

        if (err != VK_SUCCESS)
            return_main_fail(MAIN_FAIL_IMGUI_INIT);

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }


    // Now create the vertex buffers, that are constant across frames
    struct Model ground_model = { 0 };
    struct Model sphere_model = { 0 };
    struct Model cube_model = { 0 };

    WCHAR letters[26 + 3 ][2] = { 0 };
    // 26 letters , both cases and 10 digits
    for (int i = 0; i < 26; ++i)
        letters[i][0] = i + L'A';
    //for (int i = 0; i < 26; ++i)
    //    letters[i + 26][0] = i + 'a';
    //for (int i = 0; i < 10; ++i)
    //    letters[i + 26 * 2][0] = i + '0';
    letters[26][0] = L'&';
    letters[27][0] = L'ि';
    letters[28][0] = L'ब';

    struct Model letter_models[COUNT_OF(letters) ] = { 0 };


    {
        GenerateModelOutput out;
        out = load_cuboid_aa(ptr_stk_allocr, stk_offset,
                             (Vec3){ 1000.f, 0.f, 1000.f });
        if (!out.vertices || !out.indices)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        if (create_model(ptr_alloc_callbacks,
                         (CreateModelParam){
                           .device = device.device,
                           .p_allocr = &gpu_mem_allocr,
                           .index_count = out.index_count,
                           .indices_list = out.indices,
                           .vertex_count = out.vertex_count,
                           .vertices_list = out.vertices,
                         },
                         &ground_model) < 0)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        out = load_cuboid_aa(ptr_stk_allocr, stk_offset,
                             (Vec3){ 100.f, 100.f, 100.f });
        if (!out.vertices || !out.indices)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        if (create_model(ptr_alloc_callbacks,
                         (CreateModelParam){
                           .device = device.device,
                           .p_allocr = &gpu_mem_allocr,
                           .index_count = out.index_count,
                           .indices_list = out.indices,
                           .vertex_count = out.vertex_count,
                           .vertices_list = out.vertices,
                         },
                         &cube_model) < 0)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        out = load_sphere_uv(ptr_stk_allocr, stk_offset, 10, 50);
        if (!out.vertices || !out.indices)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        if (create_model(ptr_alloc_callbacks,
                         (CreateModelParam){
                           .device = device.device,
                           .p_allocr = &gpu_mem_allocr,
                           .index_count = out.index_count,
                           .indices_list = out.indices,
                           .vertex_count = out.vertex_count,
                           .vertices_list = out.vertices,
                         },
                         &sphere_model) < 0)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        for (size_t i = 0; i < COUNT_OF(letter_models); ++i) {
            out = load_text_character(ptr_stk_allocr, stk_offset,
                                      letters[i][0],
                                      (Vec3){ 0.f, 0.f, 200.f });
            if (!out.vertices || !out.indices)
                return_main_fail(MAIN_FAIL_MODEL_LOAD);

            if (create_model(ptr_alloc_callbacks,
                             (CreateModelParam){
                               .device = device.device,
                               .p_allocr = &gpu_mem_allocr,
                               .index_count = out.index_count,
                               .indices_list = out.indices,
                               .vertex_count = out.vertex_count,
                               .vertices_list = out.vertices,
                             },
                             letter_models + i) < 0)
                return_main_fail(MAIN_FAIL_MODEL_LOAD);
        }
    }

    // Miscellaneous data + object data
    struct Object3D gnd_obj = {
        .ptr_model = &ground_model,
        .translate = (Vec3){ 0.f, 300.f, 0.f },
        .rotate = (Vec3){ 0 },
        .scale = (Vec3){ 10.f, 1.f, 10.f },
        .color = (Vec3){ 0.4f, 0.8f, 0.2f },
    };
    struct Object3D scene_objs[100];
    int obj_count = 0;
    int active_obj = -1;

    Vec3 active_rotate_deg = { 0 };
    Vec3 active_translate = { 0 };
    Vec3 active_scale = { 1.f, 1.f, 1.f };
    float active_scale_pow = 0;
    Vec3 active_object_col = { 0.5f, 0.5f, 0.5f };

    winproc_data.qweasd = &active_translate;
    winproc_data.uiojkl = &active_rotate_deg;
    winproc_data.scroll = &active_scale_pow;

    Vec3 world_min = { -300, -300, -400 };
    Vec3 world_max = { 300, 300, 400 };
    float fov = M_PI / 3.f;

    Vec3 light_pos = { 0.f, 0, -800.f };
    Vec3 light_col = { 1.f, 1.f, 1.f };
    Vec3 clear_col = { 0.1f, 0.2f, 0.4f };

    MSG msg = { 0 };
    while (msg.message != WM_QUIT) {


        while ((PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Setup miscelannous data if needed
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplWin32_NewFrame();
            igNewFrame();

            active_scale.x = active_scale.y = active_scale.z =
              powf(1.1f, active_scale_pow);

            world_min.x = -winproc_data.width / 2.f;
            world_min.y = -winproc_data.height / 2.f;

            world_max.x = winproc_data.width / 2.f;
            world_max.y = winproc_data.height / 2.f;
            gnd_obj.translate.y = world_max.y;

            bool true_val = true;
            bool active_changed = false;

            {
                igBegin("3D Object Info", &true_val, 0);
                igText("Total objects in scene : %d ", obj_count);
                igText("Current object index : %d ", active_obj);
                if (igButton("Create Sphere", (ImVec2){ 0 })) {
                    active_obj = obj_count++;
                    scene_objs[active_obj] = (struct Object3D){
                        .ptr_model = &sphere_model,
                        .color = (Vec3){ 1.0f, 1.0f, 1.0f },
                        .rotate = (Vec3){ 0 },
                        .scale = (Vec3){ 1.0f, 1.0f, 1.0f },
                        .translate = (Vec3){ 0 },
                    };
                    active_changed = true;
                }
                if (igButton("Create Cube", (ImVec2){ 0 })) {
                    active_obj = obj_count++;
                    scene_objs[active_obj] = (struct Object3D){
                        .ptr_model = &cube_model,
                        .color = (Vec3){ 1.f, 1.f, 1.f },
                        .rotate = (Vec3){ 0 },
                        .translate = (Vec3){ 0 },
                        .scale = (Vec3){ 1.f, 1.f, 1.f },
                    };
                    active_changed = true;
                }

                if (igBeginCombo("Create Letter", "Choose Letter",
                                 0)) {

                    for (size_t i = 0; i < COUNT_OF(letters); ++i) {
                        if (igSelectable_Bool(letters[i], false, 0,
                                              (ImVec2){ 0 })) {
                            
                            active_obj = obj_count++;
                            scene_objs[active_obj] =
                              (struct Object3D){
                                  .ptr_model = letter_models + i,
                                  .color = (Vec3){ 1.f, 1.f, 1.f },
                                  .rotate = (Vec3){ 0 },
                                  .translate = (Vec3){ 0 },
                                  .scale = (Vec3){ 1.f, 1.f, 1.f },
                              };
                            active_changed = true;
                        }
                    }

                    igEndCombo();
                }
                

                if (igButton("Next Object", (ImVec2){ 0 })) {
                    active_obj = (active_obj + 1) % obj_count;
                    active_changed = true;
                }
                if (igButton("Previous Object", (ImVec2){ 0 })) {
                    active_obj = (active_obj - 1);
                    active_obj = (active_obj < 0) ? 0 : active_obj;
                    active_changed = true;
                }

                igColorEdit3("Object Color", active_object_col.comps,
                             0);
                igInputFloat3("Object Position",
                              active_translate.comps, NULL, 0);
                igInputFloat3("Object rotations degree",
                              active_rotate_deg.comps, NULL, 0);
                igInputFloat("Object scale factor", &active_scale_pow,
                             1 / 300.f, 1 / 150.f, "%.3f", 0);
                igEnd();
            }

            {
                igBegin("Scene Info", &true_val, 0);
                igColorEdit3("Light Color", light_col.comps, 0);
                igInputFloat3("Light Position", light_pos.comps,
                              "%.3f", 0);
                igColorEdit3("Sky Color", clear_col.comps, 0);
                igSliderFloat("FOV : ", &fov, M_PI / 18.f, M_PI, NULL,
                              0);
                igColorEdit3("Ground Color", gnd_obj.color.comps, 0);
                igEnd();
            }

            if (active_changed) {
                active_translate = scene_objs[active_obj].translate;
                active_rotate_deg =
                  vec3_to_degrees(scene_objs[active_obj].rotate);
                active_scale = scene_objs[active_obj].scale;
                active_scale_pow = logf(active_scale.x) / logf(1.1f);
                active_object_col = scene_objs[active_obj].color;
            }

            if (active_obj >= 0) {
                scene_objs[active_obj].translate = vec3_add(
                  scene_objs[active_obj].translate,
                  vec3_scale_fl(
                    vec3_sub(active_translate,
                             scene_objs[active_obj].translate),
                    10.f));
                active_translate = scene_objs[active_obj].translate;
                
                scene_objs[active_obj].scale = active_scale;
                scene_objs[active_obj].rotate =
                  vec3_to_radians(active_rotate_deg);
                scene_objs[active_obj].color = active_object_col;
            }

            igRender();
        }
        {

            uint32_t img_inx = 0;
            {
                BeginRenderingOperationsParam begin_param = {
                    .device = device.device,
                    .render_pass = render_pass,
                    .framebuffer_render_extent = swapchain_extent,
                    .swapchain = curr_swapchain_data.swapchain,
                    .framebuffers = curr_swapchain_data.framebuffers,
                    .cmd_buffer =
                      rndr_cmd_buffers[curr_frame_in_flight],
                    .render_done_fence =
                      frame_fences[curr_frame_in_flight],
                    .present_done_semaphore =
                      present_done_semaphores[curr_frame_in_flight],
                    .p_img_inx = &img_inx,
                    .clear_value =
                      (VkClearValue){
                        .color = { clear_col.r, clear_col.g,
                                   clear_col.b, 1.0f } },
                };
                int res = begin_rendering_operations(begin_param);
                RecreateSwapchainParam recreation_param = {
                    .device = device.device,
                    .create_info = swapchain_creation_infos,
                    .framebuffer_render_pass = render_pass,
                    .new_win_height = winproc_data.height,
                    .new_win_width = winproc_data.width,
                    .p_img_swap_extent = &swapchain_extent,
                    .p_old_swapchain_data = &old_swapchain_data,
                    .p_new_swapchain_data = &curr_swapchain_data
                };
                if (res < 0)
                    continue;
                if (
                  res ==
                  BEGIN_RENDERING_OPERATIONS_TRY_RECREATE_SWAPCHAIN) {
                    recreate_swapchain(ptr_stk_allocr, stk_offset,
                                       ptr_alloc_callbacks,
                                       recreation_param);

                    continue;
                }


                // Here now set viewports, bind pipeline , set
                // constants, bind buffers and draw
                vkCmdSetViewport(
                  rndr_cmd_buffers[curr_frame_in_flight], 0, 1,
                  &(VkViewport){ .x = 0.f,
                                 .y = 0.f,
                                 .minDepth = 0.f,
                                 .maxDepth = 1.f,
                                 .width = swapchain_extent.width,
                                 .height = swapchain_extent.height });
                vkCmdSetScissor(
                  rndr_cmd_buffers[curr_frame_in_flight], 0, 1,
                  &(VkRect2D){ .offset = { .x = 0, .y = 0 },
                               .extent = swapchain_extent });


                vkCmdBindPipeline(
                  rndr_cmd_buffers[curr_frame_in_flight],
                  VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
            }

            {
                struct DescriptorMats des_mats;
                struct DescriptorLight des_lights;
                des_mats.view = (Mat4)MAT4_IDENTITIY;
                des_mats.proj = (Mat4)MAT4_IDENTITIY;

                des_mats.proj =
                  mat4_orthographic(world_min, world_max);

                des_mats.proj =
                  mat4_perspective(world_min, world_max, fov);

                des_lights.light_col =
                  vec4_from_vec3(light_col, 1.0f);
                des_lights.light_src = vec4_from_vec3(light_pos, 1.f);

                *(struct DescriptorMats
                    *)(g_matrix_uniform_buffers[curr_frame_in_flight]
                         .mapped_memory) = des_mats;

                *(struct DescriptorLight
                    *)(g_lights_uniform_buffers[curr_frame_in_flight]
                         .mapped_memory) = des_lights;

                vkCmdBindDescriptorSets(
                  rndr_cmd_buffers[curr_frame_in_flight],
                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                  graphics_pipeline_layout, 0, 1,
                  g_matrix_descriptor_sets + curr_frame_in_flight, 0,
                  NULL);

                vkCmdBindDescriptorSets(
                  rndr_cmd_buffers[curr_frame_in_flight],
                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                  graphics_pipeline_layout, 1, 1,
                  g_lights_descriptor_sets + curr_frame_in_flight, 0,
                  NULL);
            }

            PushConst pushes;

            pushes = object_process_push_const(gnd_obj);

            for (int i = 0; i < push_range_count; ++i) {
                vkCmdPushConstants(
                  rndr_cmd_buffers[curr_frame_in_flight],
                  graphics_pipeline_layout, push_ranges[i].stageFlags,
                  push_ranges[i].offset, push_ranges[i].size,
                  (uint8_t *)&pushes + push_ranges[i].offset);
            }

            submit_model_draw(gnd_obj.ptr_model,
                              rndr_cmd_buffers[curr_frame_in_flight]);

            for (int i = 0; i < obj_count; ++i) {
                pushes = object_process_push_const(scene_objs[i]);

                for (int i = 0; i < push_range_count; ++i) {
                    vkCmdPushConstants(
                      rndr_cmd_buffers[curr_frame_in_flight],
                      graphics_pipeline_layout,
                      push_ranges[i].stageFlags,
                      push_ranges[i].offset, push_ranges[i].size,
                      (uint8_t *)&pushes + push_ranges[i].offset);
                }

                submit_model_draw(
                  scene_objs[i].ptr_model,
                  rndr_cmd_buffers[curr_frame_in_flight]);
            }

            {

                ImGui_ImplVulkan_RenderDrawData(
                  igGetDrawData(),
                  rndr_cmd_buffers[curr_frame_in_flight],
                  VK_NULL_HANDLE);

                EndRenderingOperationsParam end_param = {
                    .device = device.device,
                    .cmd_buffer =
                      rndr_cmd_buffers[curr_frame_in_flight],
                    .graphics_queue = device.graphics_queue,
                    .present_queue = device.present_queue,
                    .swapchain = curr_swapchain_data.swapchain,
                    .img_index = img_inx,
                    .render_done_fence =
                      frame_fences[curr_frame_in_flight],
                    .render_done_semaphore =
                      render_finished_semaphores
                        [curr_frame_in_flight],
                    .present_done_semaphore =
                      present_done_semaphores[curr_frame_in_flight]
                };
                RecreateSwapchainParam recreation_param = {
                    .device = device.device,
                    .create_info = swapchain_creation_infos,
                    .framebuffer_render_pass = render_pass,
                    .new_win_height = winproc_data.height,
                    .new_win_width = winproc_data.width,
                    .p_img_swap_extent = &swapchain_extent,
                    .p_old_swapchain_data = &old_swapchain_data,
                    .p_new_swapchain_data = &curr_swapchain_data
                };
                VkResult res = end_rendering_operations(end_param);
                if (
                  res ==
                  END_RENDERING_OPERATIONS_TRY_RECREATING_SWAPCHAIN) {
                    recreate_swapchain(ptr_stk_allocr, stk_offset,
                                       ptr_alloc_callbacks,
                                       recreation_param);

                    continue;
                }

                if (old_swapchain_data.img_count)
                    old_swapchain_data.img_count--;
                curr_frame_in_flight =
                  (curr_frame_in_flight + 1) % max_frames_in_flight;
            }
        }
    }


    failure = MAIN_FAIL_OK;

    vkDeviceWaitIdle(device.device);

cleanup_phase:
    switch (failure) {
        // Those who take in err_code, will be below their fail
        // case label Else they will be above thier fail case
        // label
        err_code = 0;
    case MAIN_FAIL_OK:

        err_code = 0;
    case MAIN_FAIL_MODEL_LOAD:
        clear_model(ptr_alloc_callbacks, device.device,
                    &ground_model);
        clear_model(ptr_alloc_callbacks, device.device,
                    &sphere_model);
        clear_model(ptr_alloc_callbacks, device.device, &cube_model);
        for (size_t i = 0; i < COUNT_OF(letter_models); ++i)
            clear_model(ptr_alloc_callbacks, device.device,
                        letter_models + i);


        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        igDestroyContext(NULL);
        err_code = 0;
    case MAIN_FAIL_IMGUI_INIT:


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
    case MAIN_FAIL_DESCRIPTOR_ALLOC_AND_BIND:


        err_code = 0;
    case MAIN_FAIL_DESCRIPTOR_MEM_ALLOC:
        g_matrix_descriptor_sets = g_lights_descriptor_sets = NULL;

        err_code = 0;
    case MAIN_FAIL_DESCRIPTOR_BUFFER:
        for (int i = 0; i < max_frames_in_flight; ++i) {
            if (g_matrix_uniform_buffers[i].buffer)
                vkDestroyBuffer(device.device,
                                g_matrix_uniform_buffers[i].buffer,
                                ptr_alloc_callbacks);
            if (g_lights_uniform_buffers[i].buffer)
                vkDestroyBuffer(device.device,
                                g_lights_uniform_buffers[i].buffer,
                                ptr_alloc_callbacks);
        }


        err_code = 0;
    case MAIN_FAIL_DESCRIPTOR_BUFFER_MEM_ALLOC:
        g_matrix_uniform_buffers = g_lights_uniform_buffers = NULL;

        err_code = 0;
    case MAIN_FAIL_DESCRIPTOR_POOL:
        clear_descriptor_pool(
          ptr_alloc_callbacks,
          (ClearDescriptorPoolParam){
            .device = device.device,
            .p_descriptor_pool = &g_descriptor_pool,
          },
          err_code);

        err_code = 0;
    case MAIN_FAIL_DESCRIPTOR_LAYOUT:
        clear_descriptor_layouts(
          ptr_alloc_callbacks,
          (ClearDescriptorLayoutsParam){
            .device = device.device,
            .p_lights_set_layout = &g_lights_layout,
            .p_matrix_set_layout = &g_matrix_layout,
          },
          err_code);

        err_code = 0;
    case MAIN_FAIL_GPU_ALLOCR:
        free_device_memory(
          ptr_alloc_callbacks,
          (FreeDeviceMemoryParam){ .device = device.device,
                                   .p_gpu_allocr = &gpu_mem_allocr },
          err_code);

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
            .framebuffer_count =
              curr_swapchain_data
                .img_count, // Use current image count for this
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

        clear_depthbuffers(
          ptr_alloc_callbacks,
          (ClearDepthbuffersParam){
            .device = device.device,
            .depth_count = curr_swapchain_data.img_count,
            .p_depth_buffers = &old_swapchain_data.depth_imgs,
            .p_depth_buffer_views = &old_swapchain_data.depth_views,
            .p_depth_memory = &old_swapchain_data.device_mem,
          },
          0);
        err_code = 0;
    case MAIN_FAIL_DEPTHBUFFERS:
        clear_depthbuffers(
          ptr_alloc_callbacks,
          (ClearDepthbuffersParam){
            .device = device.device,
            .depth_count = curr_swapchain_data.img_count,
            .p_depth_buffers = &curr_swapchain_data.depth_imgs,
            .p_depth_buffer_views = &curr_swapchain_data.depth_views,
            .p_depth_memory = &curr_swapchain_data.device_mem,
          },
          err_code);

        // Use current image count
        old_swapchain_data.img_count = curr_swapchain_data.img_count;
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


        dealloc_stack_allocator(&stk_allocr);
        err_code = 0;
    case MAIN_FAIL_STACK_ALLOCATOR:

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
