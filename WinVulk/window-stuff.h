#pragma once
#include "common-stuff.h"

struct SwapchainEntities {
    VkSwapchainKHR swapchain;

    // All following have this img_count elements if successfully
    // created
    uint32_t img_count;
    VkImage *images;
    VkImageView *img_views;
    VkDeviceMemory device_mem;
    VkImage *depth_imgs;
    VkImageView *depth_views;
    VkFramebuffer *framebuffers;
};

enum CreateSwapchainCodes {
    CREATE_SWAPCHAIN_TOP_FAIL_CODE = -0x7fff,
    CREATE_SWAPCHAIN_DEPTH_IMAGE_VIEW_CREATE_FAIL,
    CREATE_SWAPCHAIN_DEPTH_IMAGE_VIEW_ALLOC_FAIL,
    CREATE_SWAPCHAIN_DEPTH_IMAGE_DEVICE_MEM_ALLOC_FAIL,
    CREATE_SWAPCHAIN_DEPTH_IMAGE_CREATE_FAIL,
    CREATE_SWAPCHAIN_DEPTH_IMAGE_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL,
    CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL,
    CREATE_SWAPCHAIN_FAILED,
    CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE,
    CREATE_SWAPCHAIN_OK = 0,
};

struct SwapchainCreationInfo {
    VkPhysicalDevice phy_device;
    VkSurfaceKHR surface;

    uint32_t graphics_family_inx;
    uint32_t present_family_inx;

    uint32_t min_image_count;
    VkFormat depth_stencil_format;
    VkSurfaceFormatKHR surface_format;
    VkSurfaceTransformFlagBitsKHR img_pre_transform;
    VkPresentModeKHR present_mode;
};

typedef struct {
    VkDevice device;
    int win_width;
    int win_height;

    struct SwapchainCreationInfo create_info;

    VkSwapchainKHR old_swapchain;

    struct SwapchainEntities *curr_swapchain_data;
    VkExtent2D *p_img_swap_extent;

} CreateSwapchainParam;

int create_swapchain(StackAllocator *stk_allocr, size_t stk_offset,
                     VkAllocationCallbacks *alloc_callbacks,
                     CreateSwapchainParam param) {

    VkResult result = VK_SUCCESS;

    VkSurfaceCapabilitiesKHR surf_capa;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      param.create_info.phy_device, param.create_info.surface,
      &surf_capa);

    // Choose swap extent
    //  ++++
    //   TODO:: make dpi aware, ..., maybe not needed
    //  ++++
    // Now just use set width and height, also currently not checked
    // anything from capabilities Also be aware of max and min extent
    // set to numeric max of uint32_t
    if (surf_capa.currentExtent.width != -1 &&
        surf_capa.currentExtent.height != -1) {

        *param.p_img_swap_extent = surf_capa.currentExtent;

    } else {
        param.p_img_swap_extent->width = param.win_width;
        param.p_img_swap_extent->height = param.win_height;
    }

    if (!param.p_img_swap_extent->width ||
        !param.p_img_swap_extent->height) {
        return CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE;
    }

    // An array of queue family indices used
    uint32_t indices_array[] = {
        param.create_info.graphics_family_inx,
        param.create_info.present_family_inx
    };

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = param.create_info.surface,
        .minImageCount = param.create_info.min_image_count,
        .imageFormat = param.create_info.surface_format.format,
        .imageColorSpace =
          param.create_info.surface_format.colorSpace,
        .imageExtent = *param.p_img_swap_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = param.create_info.img_pre_transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = param.create_info.present_mode,
        .clipped = VK_TRUE, // This should be false only if pixels
                            // have to re read
        .oldSwapchain = param.old_swapchain,
        .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
        // Here , for exclusive sharing mode it is  optional; else for
        // concurrent, there has to be at least two different queue
        // families, and all should be specified to share the images
        // amoong
        .queueFamilyIndexCount = COUNT_OF(indices_array),
        .pQueueFamilyIndices = indices_array
    };

    if (indices_array[0] == indices_array[1]) {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    result = vkCreateSwapchainKHR(
      param.device, &create_info, alloc_callbacks,
      &(param.curr_swapchain_data->swapchain));

    if (result != VK_SUCCESS)
        return CREATE_SWAPCHAIN_FAILED;

    result = vkGetSwapchainImagesKHR(
      param.device, param.curr_swapchain_data->swapchain,
      &(param.curr_swapchain_data->img_count), NULL);

    if ((result != VK_SUCCESS) ||
        (param.curr_swapchain_data->img_count == 0))
        return CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL;

    param.curr_swapchain_data->images =
      malloc(param.curr_swapchain_data->img_count * sizeof(VkImage));

    if (!param.curr_swapchain_data->images)
        return CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL;
    vkGetSwapchainImagesKHR(param.device,
                            param.curr_swapchain_data->swapchain,
                            &(param.curr_swapchain_data->img_count),
                            param.curr_swapchain_data->images);


    param.curr_swapchain_data->img_views = malloc(
      param.curr_swapchain_data->img_count * sizeof(VkImageView));
    if (!param.curr_swapchain_data->img_views)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL;
    memset(param.curr_swapchain_data->img_views, 0,
           param.curr_swapchain_data->img_count *
             sizeof(VkImageView));
    for (size_t i = 0; i < param.curr_swapchain_data->img_count;
         ++i) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = param.curr_swapchain_data->images[i],

            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = param.create_info.surface_format.format,

            .components =
              (VkComponentMapping){
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY },

            .subresourceRange =
              (VkImageSubresourceRange){ .aspectMask =
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1 }

        };

        result = vkCreateImageView(
          param.device, &create_info, alloc_callbacks,
          param.curr_swapchain_data->img_views + i);
        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL;

    param.curr_swapchain_data->depth_imgs =
      malloc(param.curr_swapchain_data->img_count * sizeof(VkImage));
    if (!param.curr_swapchain_data->depth_imgs)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL;

    VkImageCreateInfo depth_imgs_create = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,

    };


    return CREATE_SWAPCHAIN_OK;
}

typedef struct {
    VkDevice device;
    struct SwapchainEntities *p_swapchain_data;
} ClearSwapchainParam;
void clear_swapchain(const VkAllocationCallbacks *alloc_callbacks,
                     ClearSwapchainParam param, int err_codes) {

    switch (err_codes) {

    case CREATE_SWAPCHAIN_OK:
    case CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL:
        if (param.p_swapchain_data->img_views) {
            for (int i = 0; i < param.p_swapchain_data->img_count;
                 ++i) {
                if (param.p_swapchain_data->img_views[i])
                    vkDestroyImageView(
                      param.device,
                      param.p_swapchain_data->img_views[i],
                      alloc_callbacks);
            }

            free(param.p_swapchain_data->img_views);
        }
    case CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL:
        param.p_swapchain_data->img_views = NULL;

        free(param.p_swapchain_data->images);

    case CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL:
        param.p_swapchain_data->images = NULL;
        param.p_swapchain_data->img_count = 0;

    case CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL:
        vkDestroySwapchainKHR(param.device,
                              param.p_swapchain_data->swapchain,
                              alloc_callbacks);

    case CREATE_SWAPCHAIN_FAILED:
        param.p_swapchain_data->swapchain = VK_NULL_HANDLE;
    case CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE:

        break;
    }
}

enum CreateDepthbuffersCodes {
    CREATE_DEPTHBUFFERS_BASE_FAIL = -0x7fff,
    CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL,
    CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE,
    CREATE_DEPTHBUFFERS_IMAGE_FAIL,
    CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL,
    CREATE_DEPTHBUFFERS_OK = 0,
};

typedef struct {
    VkDevice device;
    VkPhysicalDevice phy_device;
    size_t depth_count;
    VkExtent2D img_extent;
    VkFormat depth_img_format;


    VkImage **p_depth_buffers;
    VkImageView **p_depth_buffer_views;
    VkDeviceMemory *p_depth_memory;
} CreateDepthbuffersParam;

int create_depthbuffers(StackAllocator *stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks *alloc_callbacks,
                        CreateDepthbuffersParam param) {
    VkResult result = VK_SUCCESS;

    *param.p_depth_buffers =
      malloc(param.depth_count * sizeof(VkImage));
    if (!(*param.p_depth_buffers))
        return CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL;

    VkImageCreateInfo img_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .extent =
          (VkExtent3D){
            .depth = 1,
            .width = param.img_extent.width,
            .height = param.img_extent.height,
          },
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = param.depth_img_format,
        .imageType = VK_IMAGE_TYPE_2D,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    for (size_t i = 0; i < param.depth_count; ++i) {
        (*param.p_depth_buffers)[i] = VK_NULL_HANDLE;
        result =
          vkCreateImage(param.device, &img_create_info,
                        alloc_callbacks, *param.p_depth_buffers + i);
        if (result != VK_SUCCESS)
            return CREATE_DEPTHBUFFERS_IMAGE_FAIL;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(
      param.device, param.p_depth_buffers[0][0], &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(param.phy_device, &mem_props);

    uint32_t mem_type_inx;
    for (mem_type_inx = 0; mem_type_inx < mem_props.memoryTypeCount;
         ++mem_type_inx) {
        if ((mem_reqs.memoryTypeBits & (1 << mem_type_inx)) &&
            (mem_props.memoryTypes[mem_type_inx].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            break;
    }

    if (mem_type_inx == mem_props.memoryTypeCount)
        return CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE;

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = mem_type_inx,
        .allocationSize =
          align_up_(mem_reqs.size, mem_reqs.alignment) *
          param.depth_count,
    };

    result = vkAllocateMemory(param.device, &alloc_info,
                              alloc_callbacks, param.p_depth_memory);
    if (result != VK_SUCCESS)
        return CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL;

    for (size_t i = 0; i < param.depth_count; ++i) {
        result = vkBindImageMemory(
          param.device, (*param.p_depth_buffers)[i],
          *param.p_depth_memory,
          (VkDeviceSize)(i *
                         align_up_(mem_reqs.size,
                                   mem_reqs.alignment)));
        if (result != VK_SUCCESS)
            return CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL;
    }

    *param.p_depth_buffer_views =
      malloc(param.depth_count * sizeof(VkImageView));
    if (!*param.p_depth_buffer_views)
        return CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL;

    for (size_t i = 0; i < param.depth_count; ++i) {
        (*param.p_depth_buffer_views)[i] = VK_NULL_HANDLE;
        VkImageViewCreateInfo view_create = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = param.depth_img_format,
            .components = {
                0                
            },
            .image = (*param.p_depth_buffers)[i],
            .subresourceRange = {
                .baseArrayLayer = 0,
                .levelCount = 1,
                .baseMipLevel = 0,
                .layerCount =  1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            }
            
        };

        result = vkCreateImageView(param.device, &view_create,
                                   alloc_callbacks,
                                   param.p_depth_buffer_views[0] + i);
        if (result != VK_SUCCESS)
            return CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL;
    }


    return CREATE_DEPTHBUFFERS_OK;
}

typedef struct {
    VkDevice device;
    size_t depth_count;

    VkImage **p_depth_buffers;
    VkImageView **p_depth_buffer_views;
    VkDeviceMemory *p_depth_memory;
}ClearDepthbuffersParam;

void clear_depthbuffers(const VkAllocationCallbacks *alloc_callbacks,
                        ClearDepthbuffersParam param, int err_code) {

    switch(err_code) {

        case CREATE_DEPTHBUFFERS_OK:


        case CREATE_DEPTHBUFFERS_IMAGE_VIEW_FAIL:
        if (param.p_depth_buffer_views[0])
        for (size_t i = 0; i < param.depth_count; ++i)
            if ((*param.p_depth_buffer_views)[i])
                vkDestroyImageView(param.device,
                                   (*param.p_depth_buffer_views)[i],
                                   alloc_callbacks);
        if (param.p_depth_buffer_views[0])
            free(*param.p_depth_buffer_views);
        case CREATE_DEPTHBUFFERS_IMAGE_VIEW_MEM_ALLOC_FAIL:
            *param.p_depth_buffer_views = NULL;

        case CREATE_DEPTHBUFFERS_IMAGE_DEVICE_MEM_BIND_FAIL:


        case CREATE_DEPTHBUFFERS_DEVICE_MEM_ALLOC_FAIL:
            vkFreeMemory(param.device, *param.p_depth_memory,
                     alloc_callbacks);

        case CREATE_DEPTHBUFFERS_DEVICE_MEM_INAPPROPRIATE:


        case CREATE_DEPTHBUFFERS_IMAGE_FAIL:
            if (param.p_depth_buffers[0])
            for(size_t i = 0; i < param.depth_count; ++i) {
            if (param.p_depth_buffers[0][i])
                vkDestroyImage(param.device,
                               param.p_depth_buffers[0][i],
                               alloc_callbacks);
            }

        if (param.p_depth_buffers[0])
            free(*param.p_depth_buffers);
        case CREATE_DEPTHBUFFERS_IMAGE_MEM_ALLOC_FAIL:
            *param.p_depth_buffers = NULL;

        case CREATE_DEPTHBUFFERS_BASE_FAIL:

            break;
    }

}

enum CreateFramebuffersCodes {
    CREATE_FRAMEBUFFERS_FAILED = -0x7fff,
    CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED,
    CREATE_FRAMEBUFFERS_OK = 0,
};

typedef struct {
    VkDevice device;

    VkExtent2D framebuffer_extent;
    VkRenderPass compatible_render_pass;
    uint32_t framebuffer_count;
    VkImageView *img_views;
    VkImageView *depth_views;

    VkFramebuffer **p_framebuffers;

} CreateFramebuffersParam;

int create_framebuffers(StackAllocator *stk_allocr, size_t stk_offset,
                        const VkAllocationCallbacks *alloc_callbacks,
                        CreateFramebuffersParam param) {

    VkResult result = VK_SUCCESS;

    param.p_framebuffers[0] =
      malloc(param.framebuffer_count * sizeof(VkFramebuffer));

    if (!param.p_framebuffers[0])
        return CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED;

    memset(param.p_framebuffers[0], 0,
           param.framebuffer_count * sizeof(VkFramebuffer));
    for (int i = 0; i < param.framebuffer_count; ++i) {
        VkImageView img_attachments[] = { param.img_views[i],
                                          param.depth_views[i] };
        VkFramebufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = param.compatible_render_pass,
            .attachmentCount = COUNT_OF(img_attachments),
            .pAttachments = img_attachments,
            .width = param.framebuffer_extent.width,
            .height = param.framebuffer_extent.height,
            .layers = 1,
        };
        result = vkCreateFramebuffer(param.device, &create_info,
                                     alloc_callbacks,
                                     param.p_framebuffers[0] + i);

        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_FRAMEBUFFERS_FAILED;

    return CREATE_FRAMEBUFFERS_OK;
}

typedef struct {
    VkDevice device;
    uint32_t framebuffer_count;

    VkFramebuffer **p_framebuffers;
} ClearFramebuffersParam;
void clear_framebuffers(const VkAllocationCallbacks *alloc_callbacks,
                        ClearFramebuffersParam param, int err_codes) {

    switch (err_codes) {
    case CREATE_FRAMEBUFFERS_OK:
    case CREATE_FRAMEBUFFERS_FAILED:
        if (param.p_framebuffers[0]) {
            for (int i = 0; i < param.framebuffer_count; ++i) {
                if (param.p_framebuffers[0][i])
                    vkDestroyFramebuffer(param.device,
                                         param.p_framebuffers[0][i],
                                         alloc_callbacks);
            }

            free(param.p_framebuffers[0]);
        }
    case CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED:
        param.p_framebuffers[0] = NULL;
    }
}

enum RecreateSwapchainCodes {
    RECREATE_SWAPCHAIN_CREATE_SWAPCHAIN_ERR = -0x7fff,
    RECREATE_SWAPCHAIN_CREATE_FRAMEBUFFER_ERR,
    RECREATE_SWAPCHAIN_OK = 0,
};

typedef struct {
    VkDevice device;
    int new_win_width;
    int new_win_height;
    VkRenderPass framebuffer_render_pass;
    struct SwapchainCreationInfo create_info;

    VkExtent2D *p_img_swap_extent;
    struct SwapchainEntities *p_old_swapchain_data;
    struct SwapchainEntities *p_new_swapchain_data;
} RecreateSwapchainParam;

int recreate_swapchain(StackAllocator *stk_allocr, size_t stk_offset,
                       VkAllocationCallbacks *alloc_callbacks,
                       RecreateSwapchainParam param) {

    if (param.p_old_swapchain_data->swapchain) {
        if (param.p_old_swapchain_data->img_count > 0)
            vkDeviceWaitIdle(param.device);
        param.p_old_swapchain_data->img_count =
          param.p_new_swapchain_data->img_count;

        ClearFramebuffersParam clear_fb = {
            .device = param.device,
            .framebuffer_count =
              param.p_old_swapchain_data->img_count,
            .p_framebuffers =
              &param.p_old_swapchain_data->framebuffers
        };
        clear_framebuffers(alloc_callbacks, clear_fb, 0);

        ClearDepthbuffersParam clear_db = {
            .device = param.device,
            .depth_count = param.p_old_swapchain_data->img_count,
            .p_depth_buffers =
              &param.p_old_swapchain_data->depth_imgs,
            .p_depth_buffer_views =
              &param.p_old_swapchain_data->depth_views,
            .p_depth_memory = &param.p_old_swapchain_data->device_mem,
        };
        clear_depthbuffers(alloc_callbacks, clear_db, 0);

        ClearSwapchainParam clear_sc = {
            .device = param.device,
            .p_swapchain_data = param.p_old_swapchain_data,
        };
        clear_swapchain(alloc_callbacks, clear_sc, 0);
    }

    param.p_old_swapchain_data[0] = param.p_new_swapchain_data[0];

    int err_code = 0;

    CreateSwapchainParam create_sc = {
        .device = param.device,
        .win_height = param.new_win_height,
        .win_width = param.new_win_width,
        .create_info = param.create_info,
        .p_img_swap_extent = param.p_img_swap_extent,
        .old_swapchain = param.p_old_swapchain_data->swapchain,
        .curr_swapchain_data = param.p_new_swapchain_data,
    };

    err_code = create_swapchain(stk_allocr, stk_offset,
                                alloc_callbacks, create_sc);
    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with
        // swapchain unchanged Also clear the swapchain, and reassign
        // old swapchain to current again
        ClearSwapchainParam clear_sc = {
            .device = param.device,
            .p_swapchain_data = param.p_new_swapchain_data,
        };
        clear_swapchain(alloc_callbacks, clear_sc, err_code);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) =
          (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_SWAPCHAIN_ERR;
    }
    CreateDepthbuffersParam create_db = {
        .device = param.device,
        .phy_device = param.create_info.phy_device,
        .depth_count = param.p_new_swapchain_data->img_count,
        .img_extent = *param.p_img_swap_extent,
        .depth_img_format = param.create_info.depth_stencil_format,
        .p_depth_buffers = &param.p_new_swapchain_data->depth_imgs,
        .p_depth_buffer_views =
          &param.p_new_swapchain_data->depth_views,
        .p_depth_memory = &param.p_new_swapchain_data->device_mem,
    };
    err_code = create_depthbuffers(stk_allocr, stk_offset,
                                   alloc_callbacks, create_db);
    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with
        // swapchain unchanged Also clear the swapchain, and reassign
        // old swapchain to current again
        clear_depthbuffers(
          alloc_callbacks,
          (ClearDepthbuffersParam){
            .device = param.device,
            .depth_count = param.p_new_swapchain_data->img_count,
            .p_depth_buffers =
              &param.p_new_swapchain_data->depth_imgs,
            .p_depth_buffer_views =
              &param.p_new_swapchain_data->depth_views,
            .p_depth_memory = &param.p_new_swapchain_data->device_mem,
          },
          err_code);
        ClearSwapchainParam clear_sc = {
            .device = param.device,
            .p_swapchain_data = param.p_new_swapchain_data,
        };
        clear_swapchain(alloc_callbacks, clear_sc, 0);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) =
          (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_SWAPCHAIN_ERR;
    }

    CreateFramebuffersParam create_fb = {
        .device = param.device,
        .compatible_render_pass = param.framebuffer_render_pass,
        .framebuffer_count = param.p_new_swapchain_data->img_count,
        .framebuffer_extent = param.p_img_swap_extent[0],
        .img_views = param.p_new_swapchain_data->img_views,
        .depth_views = param.p_new_swapchain_data->depth_views,
        .p_framebuffers = &(param.p_new_swapchain_data->framebuffers),
    };
    err_code = create_framebuffers(stk_allocr, stk_offset,
                                   alloc_callbacks, create_fb);

    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with
        // swapchain unchanged Also clear the swapchain, and reassign
        // old swapchain to current again

        ClearFramebuffersParam clear_fb = {
            .device = param.device,
            .framebuffer_count =
              param.p_new_swapchain_data->img_count,
            .p_framebuffers =
              &(param.p_new_swapchain_data->framebuffers),
        };

        clear_framebuffers(alloc_callbacks, clear_fb, err_code);

        ClearSwapchainParam clear_sc = {
            .device = param.device,
            .p_swapchain_data = param.p_new_swapchain_data,
        };
        clear_swapchain(alloc_callbacks, clear_sc, 0);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) =
          (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_FRAMEBUFFER_ERR;
    }

    return RECREATE_SWAPCHAIN_OK;
}
