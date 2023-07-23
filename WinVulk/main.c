#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "common-stuff.h"
#include "device-mem-stuff.h"
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
        if(msg == WM_MOUSEWHEEL) {
            if(pdata->scroll) {
                pdata->scroll[0] +=
                  (float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            }
        }
    }

    return DefWindowProc(h_wnd, msg, wparam, lparam);
}

#include "misc_tools.h"

#pragma warning(push, 3)
#pragma warning(default : 4703 4700)
int main(int argc, char *argv[]) {

    enum MainFailCodes {
        MAIN_FAIL_ALL = -0x7fff,

        MAIN_FAIL_GRAPHICS_PIPELINE,
        MAIN_FAIL_GRAPHICS_PIPELINE_LAYOUT,

        MAIN_FAIL_MODEL_LOAD,

        MAIN_FAIL_GPU_ALLOCR,

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
                        WS_OVERLAPPEDWINDOW, 20, 10, 600, 600, NULL,
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
    }

    // Create Render Pass
    VkRenderPass render_pass = VK_NULL_HANDLE;
    {
        CreateRenderPassParam param = {
            .device = device.device,
            .img_format = device.img_format.format,
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

    // For now , required memory type will be hardcoded, later must be
    // properly evaluated, maybe after textures are implemented
    uint32_t memory_type_flags = 255;
    size_t total_gpu_memory = sizeof(VertexInput) * 200;
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


    // Now create the vertex buffers, that are constant across frames
    struct Model square_model = { 0 };
    {
        float r1_3 = sqrtf(1.f/3.f);

        const VertexInput major_vertex_sample_data[] = {
            { { -50.f, -50.f, 50.f },
              { -r1_3, -r1_3, -r1_3 },
              { 1.f, 1.f, 1.f },
              { 0.f, 0.f } },
            { { 50.f, 50.f, 50.f },
              { r1_3, r1_3, -r1_3 },
              { 1.f, 1.f, 1.f },
              { 1.f, 1.f } },
            { { -50.f, 50.f, 50.f },
              { -r1_3, r1_3, -r1_3 },
              { 1.f, 1.f, 1.f },
              { 0.f, 1.f } },
            { { 50.f, -50.f, 50.f },
              { r1_3, -r1_3, -r1_3 },
              { 1.f, 1.f, 1.f },
              { 1.f, 0.f } },
            { { -50.f, -50.f, 150.f },
              { -r1_3, -r1_3, r1_3 },
              { 1.f, 1.f, 1.f },
              { 0.f, 0.f } },
            { { 50.f, 50.f, 150.f },
              { r1_3, r1_3, r1_3 },
              { 1.f, 1.f, 1.f },
              { 1.f, 1.f } },
            { { -50.f, 50.f, 150.f },
              { -r1_3, r1_3, r1_3 },
              { 1.f, 1.f, 1.f },
              { 0.f, 1.f } },
            { { 50.f, -50.f, 150.f },
              { r1_3, -r1_3, r1_3 },
              { 1.f, 1.f, 1.f },
              { 1.f, 0.f } },
        };

        const uint16_t major_index_sample_data[] = {
            0, 1, 2, 1, 0, 3,
            4,6,5,5,7,4,
            1,3,7,7,5,1,
            4,0,2,2,6,4,
            2,5,6,2,1,5,
            0,4,7,7,3,0,
        };

        //generate a cylinder ? 
        // generate two cirles, / polygons, with center also as vertices
        // with radially outwards normals
        // add +-z to those
        // renormalize the normals
        // 
        // for indices:
        // one triangle for each end faces' center and two edges
        // two triangles for each side
        //

        const int divs = 3;
        const int n_verts = divs * 4 + 2;
        const int n_indxs = 3 * 8 * divs;
        const int radius = 200;
        const int depth = 300;
        const int bevel = 10;
        const Vec3 front_col = {
            .r = 1.0f,
            .g = 1.0f,
            .b = 0.0f,
        };
        const Vec3 back_col = {
            .r = 0.0f,
            .g = 0.0f,
            .b = 1.0f,
        };

        size_t v_size = n_verts * sizeof(VertexInput);
        size_t i_size = n_indxs * sizeof(uint16_t);

        VertexInput *verts =
          stack_allocate(&stk_allocr, &stk_offset, v_size, 4);
        uint16_t *indxs =
          stack_allocate(&stk_allocr, &stk_offset, i_size, 4);

        int inx_count = 0;
        for (int i = 0; i < divs; ++i) {

            float nx = cosf(i * M_PI * 2.f/divs);
            float ny = sinf(i * M_PI * 2.f/divs);
            float fx = radius * nx;
            float fy = radius * ny;
            float bx = (radius -bevel)* nx;
            float by = (radius -bevel)* ny;
            {

                // For front
                verts[i + 0 * divs].pos = (Vec3){
                    .x = bx, .y = by, .z = -(depth / 2 + bevel)
                };
                verts[i + 0 * divs].normal =
                  (Vec3){ .x = 0.f, .y = 0.f, .z = -1.f };
                verts[i + 0 * divs].color = front_col;

                // For bevel to front

                verts[i + 1 * divs].pos =
                  (Vec3){ .x = fx, .y = fy, .z = -(depth / 2) };
                verts[i + 1 * divs].normal =
                  (Vec3){ .x = nx, .y = ny, .z = 0.f };
                verts[i + 1 * divs].color = front_col;


                // For bevel to back

                verts[i + 2 * divs].pos =
                  (Vec3){ .x = fx, .y = fy, .z = (depth / 2) };
                verts[i + 2 * divs].normal =
                  (Vec3){ .x = nx, .y = ny, .z = 0.f };
                verts[i + 2 * divs].color = back_col;


                // For back

                verts[i + 3 * divs].pos = (Vec3){
                    .x = bx, .y = by, .z = (depth / 2 + bevel)
                };
                verts[i + 3 * divs].normal =
                  (Vec3){ .x = 0.f, .y = 0.f, .z = 1.f };
                verts[i + 3 * divs].color = back_col;

            }


            //For front face
            indxs[inx_count++] = i;
            indxs[inx_count++] = (i + 1) % divs;
            indxs[inx_count++] = n_verts - 2;
            
            //For front bevel
            indxs[inx_count++] = i ;
            indxs[inx_count++] = i + divs ;
            indxs[inx_count++] = (i + 1) % divs + divs ;
            
            indxs[inx_count++] = (i + 1) % divs + divs ;
            indxs[inx_count++] = (i + 1) % divs ;
            indxs[inx_count++] = i ;


            //For side
            indxs[inx_count++] = i  + divs;
            indxs[inx_count++] = i + divs*2;
            indxs[inx_count++] = (i + 1) % divs + divs*2;

            indxs[inx_count++] = (i + 1) % divs + divs*2;
            indxs[inx_count++] = (i + 1) % divs + divs;
            indxs[inx_count++] = i +divs;

            //For back bevel
            indxs[inx_count++] = i + divs * 2;
            indxs[inx_count++] = i + divs * 3;
            indxs[inx_count++] = (i + 1) % divs + divs * 3;
            
            indxs[inx_count++] = (i + 1) % divs + divs * 3;
            indxs[inx_count++] = (i + 1) % divs + divs * 2;
            indxs[inx_count++] = i + divs * 2;

            //For back face
            indxs[inx_count++] = divs*3 + (i + 1) % divs;
            indxs[inx_count++] = divs*3 + i;
            indxs[inx_count++] = n_verts - 1;


        }

        verts[n_verts - 2].normal = (Vec3){ 0.f, 0.f, -1.f };
        verts[n_verts - 1].normal = (Vec3){ 0.f, 0.f, 1.f };
        verts[n_verts - 2].pos = (Vec3){ 0.f, 0.f, -(bevel+depth/2) };
        verts[n_verts - 1].pos = (Vec3){ 0.f, 0.f,  (bevel+depth/2) };
        verts[n_verts - 2].color = front_col;
        verts[n_verts - 1].color = back_col;
        

        //Eh, make this cylinder a sphere 

        /*for(int i = 0; i < n_verts; ++i) {
            verts[i].pos =
              vec3_scale_fl(vec3_normalize(verts[i].pos), radius);
        
            verts[i].normal = vec3_normalize(verts[i].pos);
        }*/

        if (create_model(
              ptr_alloc_callbacks,
              (CreateModelParam){
                           .device = device.device,
                           .p_allocr = &gpu_mem_allocr,
                           .index_count = n_indxs,
                           .indices_list = indxs,
                           .vertex_count = n_verts,
                           .vertices_list = verts,
                         },
                         &square_model) < 0)
            return_main_fail(MAIN_FAIL_MODEL_LOAD);

        //if (create_model(
        //      ptr_alloc_callbacks,
        //      (CreateModelParam){
        //        .device = device.device,
        //        .p_allocr = &gpu_mem_allocr,
        //        .index_count = COUNT_OF(major_index_sample_data),
        //        .indices_list = major_index_sample_data,
        //        .vertex_count = COUNT_OF(major_vertex_sample_data),
        //        .vertices_list = major_vertex_sample_data },
        //      &square_model) < 0)
        //    return_main_fail(MAIN_FAIL_MODEL_LOAD);
    }


    // Now create the graphics pipeline layout, with only push
    // constant of a float for now
    VkPipelineLayout graphics_pipeline_layout = VK_NULL_HANDLE;
    {

        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pushConstantRangeCount = COUNT_OF(push_ranges),
            .pPushConstantRanges = push_ranges,


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
        */


        err_code = create_graphics_pipeline(
          ptr_stk_allocr, stk_offset, ptr_alloc_callbacks,
          (CreateGraphicsPipelineParam){
            .create_infos = create_infos,
            .compatible_render_pass = render_pass,
            .device = device.device,
            .pipe_layout = graphics_pipeline_layout,
            .subpass_index = 0,
            .vert_shader_file = "shaders/out/demo.vert.hlsl.spv",
            .frag_shader_file = "shaders/out/demo.frag.hlsl.spv",
            .p_pipeline = &graphics_pipeline });
        if (err_code < 0)
            return_main_fail(MAIN_FAIL_GRAPHICS_PIPELINE);
    }

    ShowWindow(winproc_data.win_handle, SW_SHOW);

    // Miscellaneous data + data to transform models
    Vec3 sq_rotate = { 0 };
    Vec3 sq_translate = { 0 };
    Vec3 sq_scale = { 1.f,1.f,1.f };
    float sq_scale_pow = 0;

    winproc_data.qweasd = &sq_translate;
    winproc_data.uiojkl = &sq_rotate;
    winproc_data.scroll = &sq_scale_pow;

    Vec3 world_min = { -300,-300,-900};
    Vec3 world_max = { 300, 300, 900 };

    Vec3 light = { 0.f, 0.f, 1.f };

    MSG msg = { 0 };
    while (msg.message != WM_QUIT) {
        // Setup miscelannous data if needed
        {
            sq_scale.x = sq_scale.y = sq_scale.z =
              powf(1.1f, sq_scale_pow);
            world_min.x = -winproc_data.width / 2.f;
            world_min.y = -winproc_data.height / 2.f;

            world_max.x = winproc_data.width / 2.f;
            world_max.y = winproc_data.height / 2.f;
        }


        while ((PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
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


            struct PushConst pushes = { 0 };
            pushes.view_proj = (Mat4)MAT4_IDENTITIY;

            Mat4 translate = mat4_translate_3(
              sq_translate.x, sq_translate.y, sq_translate.z);
            Mat4 rotate = mat4_rotation_XYZ(vec3_to_radians(sq_rotate));
            Mat4 scale =
              mat4_scale_3(sq_scale.x, sq_scale.y, sq_scale.z);

            pushes.model_mat =
              mat4_multiply_mat_3(&translate, &rotate, &scale);
            pushes.view_proj =
              mat4_orthographic(world_min, world_max);
            pushes.model_mat.cols[3] =
              (Vec4){ light.x, light.y, light.z,1.0f };

            vkCmdPushConstants(rndr_cmd_buffers[curr_frame_in_flight],
                               graphics_pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(struct PushConst), &pushes);

            submit_model_draw(&square_model,
                              rndr_cmd_buffers[curr_frame_in_flight]);

            {
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
    case MAIN_FAIL_MODEL_LOAD:
        clear_model(ptr_alloc_callbacks, device.device,
                    &square_model);

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
