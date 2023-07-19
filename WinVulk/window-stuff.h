#pragma once
#include "common-stuff.h"

struct SwapchainEntities {
    VkSwapchainKHR swapchain;

    // All following have this img_count elements if successfully created
    uint32_t img_count;
    VkImage *images;
    VkImageView *img_views;
    VkFramebuffer *framebuffers;
};
// Data like window handle, size, surface, images, framebuffers, swapchains
struct WindowData {
    HANDLE win_handle;
    int width;
    int height;

    VkSurfaceKHR surface;

    uint32_t min_img_count;
    VkSurfaceFormatKHR img_format;
    VkSurfaceTransformFlagBitsKHR img_surface_transform_flag;
    VkPresentModeKHR img_present_mode;
    VkExtent2D img_swap_extent;

    // A struct of everything that depends on swapchain
    struct SwapchainEntities curr_swapchain;

    struct SwapchainEntities old_swapchain;
};

typedef struct WindowData WindowData;

enum CreateSwapchainCodes {
    CREATE_SWAPCHAIN_FAILED = -0x7fff,
    CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE,
    CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL,
    CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL,
    CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL,
    CREATE_SWAPCHAIN_OK = 0,
};

struct SwapchainCreationInfo {
    VkPhysicalDevice phy_device;
    VkSurfaceKHR surface;

    int graphics_family_inx;
    int present_family_inx;

    uint32_t min_image_count;
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
                     VkAllocationCallbacks *alloc_callbacks, CreateSwapchainParam param) {

    VkResult result = VK_SUCCESS;

    VkSurfaceCapabilitiesKHR surf_capa;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(param.create_info.phy_device,
                                              param.create_info.surface, &surf_capa);

    // Choose swap extent
    //  ++++
    //   TODO:: make dpi aware, ..., maybe not needed
    //  ++++
    // Now just use set width and height, also currently not checked anything from
    // capabilities Also be aware of max and min extent set to numeric max of
    // uint32_t
    if (surf_capa.currentExtent.width != -1 && surf_capa.currentExtent.height != -1) {

        *param.p_img_swap_extent = surf_capa.currentExtent;

    } else {
        param.p_img_swap_extent->width = param.win_width;
        param.p_img_swap_extent->height = param.win_height;
    }

    if (!param.p_img_swap_extent->width || !param.p_img_swap_extent->height) {
        return CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE;
    }

    // An array of queue family indices used
    uint32_t indices_array[] = { param.create_info.graphics_family_inx,
                                 param.create_info.present_family_inx };

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = param.create_info.surface,
        .minImageCount = param.create_info.min_image_count,
        .imageFormat = param.create_info.surface_format.format,
        .imageColorSpace = param.create_info.surface_format.colorSpace,
        .imageExtent = *param.p_img_swap_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = param.create_info.img_pre_transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = param.create_info.present_mode,
        .clipped = VK_TRUE, // This should be false only if pixels have to re read
        .oldSwapchain = param.old_swapchain,
        .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
        // Here , for exclusive sharing mode it is  optional; else for concurrent,
        // there has to be at least two different queue families, and all should
        // be specified to share the images amoong
        .queueFamilyIndexCount = COUNT_OF(indices_array),
        .pQueueFamilyIndices = indices_array
    };

    if (indices_array[0] == indices_array[1]) {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    result = vkCreateSwapchainKHR(param.device, &create_info, alloc_callbacks,
                                  &(param.curr_swapchain_data->swapchain));

    if (result != VK_SUCCESS)
        return CREATE_SWAPCHAIN_FAILED;

    result = vkGetSwapchainImagesKHR(param.device, param.curr_swapchain_data->swapchain,
                                     &(param.curr_swapchain_data->img_count), NULL);

    if ((result != VK_SUCCESS) || (param.curr_swapchain_data->img_count == 0))
        return CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL;

    param.curr_swapchain_data->images =
      malloc(param.curr_swapchain_data->img_count * sizeof(VkImage));
    if (!param.curr_swapchain_data->images)
        return CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL;
    vkGetSwapchainImagesKHR(param.device, param.curr_swapchain_data->swapchain,
                            &(param.curr_swapchain_data->img_count),
                            param.curr_swapchain_data->images);

    param.curr_swapchain_data->img_views =
      malloc(param.curr_swapchain_data->img_count * sizeof(VkImageView));
    if (!param.curr_swapchain_data->img_views)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL;
    memset(param.curr_swapchain_data->img_views, 0,
           param.curr_swapchain_data->img_count * sizeof(VkImageView));
    for (size_t i = 0; i < param.curr_swapchain_data->img_count; ++i) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = param.curr_swapchain_data->images[i],

            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = param.create_info.surface_format.format,

            .components = (VkComponentMapping){ .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                .a = VK_COMPONENT_SWIZZLE_IDENTITY },

            .subresourceRange =
              (VkImageSubresourceRange){ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1 }

        };

        result = vkCreateImageView(param.device, &create_info, alloc_callbacks,
                                   param.curr_swapchain_data->img_views + i);
        if (result != VK_SUCCESS)
            break;
    }

    if (result != VK_SUCCESS)
        return CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL;

    return CREATE_SWAPCHAIN_OK;
}

typedef struct {
    VkDevice device;
    struct SwapchainEntities *p_swapchain_data;
} ClearSwapchainParam;
void clear_swapchain(VkAllocationCallbacks *alloc_callbacks, ClearSwapchainParam param,
                     int err_codes) {

    switch (err_codes) {

    case CREATE_SWAPCHAIN_OK:
    case CREATE_SWAPCHAIN_IMAGE_VIEW_CREATE_FAIL:
        for (int i = 0; i < param.p_swapchain_data->img_count; ++i) {
            if (param.p_swapchain_data->img_views[i])
                vkDestroyImageView(param.device, param.p_swapchain_data->img_views[i],
                                   alloc_callbacks);
        }

        free(param.p_swapchain_data->img_views);
    case CREATE_SWAPCHAIN_IMAGE_VIEW_ALLOC_FAIL:
        param.p_swapchain_data->img_views = NULL;

        free(param.p_swapchain_data->images);

    case CREATE_SWAPCHAIN_IMAGE_ALLOC_FAIL:
        param.p_swapchain_data->images = NULL;
        param.p_swapchain_data->img_count = 0;

    case CREATE_SWAPCHAIN_IMAGE_LOAD_FAIL:
        vkDestroySwapchainKHR(param.device, param.p_swapchain_data->swapchain,
                              alloc_callbacks);

    case CREATE_SWAPCHAIN_FAILED:
        param.p_swapchain_data->swapchain = VK_NULL_HANDLE;
    case CREATE_SWAPCHAIN_ZERO_SURFACE_SIZE:

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

    VkFramebuffer **p_framebuffers;

} CreateFramebuffersParam;

int create_framebuffers(StackAllocator *stk_allocr, size_t stk_offset,
                        VkAllocationCallbacks *alloc_callbacks,
                        CreateFramebuffersParam param) {

    VkResult result = VK_SUCCESS;

    param.p_framebuffers[0] = malloc(param.framebuffer_count * sizeof(VkFramebuffer));

    if (!param.p_framebuffers[0])
        return CREATE_FRAMEBUFFERS_INT_ALLOC_FAILED;

    memset(param.p_framebuffers[0], 0, param.framebuffer_count * sizeof(VkFramebuffer));
    for (int i = 0; i < param.framebuffer_count; ++i) {
        VkImageView img_attachments[] = { param.img_views[i] };
        VkFramebufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = param.compatible_render_pass,
            .attachmentCount = COUNT_OF(img_attachments),
            .pAttachments = img_attachments,
            .width = param.framebuffer_extent.width,
            .height = param.framebuffer_extent.height,
            .layers = 1,
        };
        result = vkCreateFramebuffer(param.device, &create_info, alloc_callbacks,
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
void clear_framebuffers(VkAllocationCallbacks *alloc_callbacks,
                        ClearFramebuffersParam param, int err_codes) {

    switch (err_codes) {
    case CREATE_FRAMEBUFFERS_OK:
    case CREATE_FRAMEBUFFERS_FAILED:
        for (int i = 0; i < param.framebuffer_count; ++i) {
            if (param.p_framebuffers[0][i])
                vkDestroyFramebuffer(param.device, param.p_framebuffers[0][i],
                                     alloc_callbacks);
            else
                break;
        }

        free(param.p_framebuffers[0]);
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
        param.p_old_swapchain_data->img_count = param.p_new_swapchain_data->img_count;

        ClearFramebuffersParam clear_fb = { .device = param.device,
                                            .framebuffer_count =
                                              param.p_old_swapchain_data->img_count,
                                            .p_framebuffers =
                                              &param.p_old_swapchain_data->framebuffers };
        clear_framebuffers(alloc_callbacks, clear_fb, 0);

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

    err_code = create_swapchain(stk_allocr, stk_offset, alloc_callbacks, create_sc);
    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with swapchain unchanged
        // Also clear the swapchain, and reassign old swapchain to current again
        ClearSwapchainParam clear_sc = {
            .device = param.device,
            .p_swapchain_data = param.p_new_swapchain_data,
        };
        clear_swapchain(alloc_callbacks, clear_sc, err_code);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) = (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_SWAPCHAIN_ERR;
    }

    CreateFramebuffersParam create_fb = {
        .device = param.device,
        .compatible_render_pass = param.framebuffer_render_pass,
        .framebuffer_count = param.p_new_swapchain_data->img_count,
        .framebuffer_extent = param.p_img_swap_extent[0],
        .img_views = param.p_new_swapchain_data->img_views,
        .p_framebuffers = &(param.p_new_swapchain_data->framebuffers)
    };
    err_code = create_framebuffers(stk_allocr, stk_offset, alloc_callbacks, create_fb);

    if (err_code < 0) {
        // Return if any error, like zero framebuffer size, with swapchain unchanged
        // Also clear the swapchain, and reassign old swapchain to current again

        ClearFramebuffersParam clear_fb = {
            .device = param.device,
            .framebuffer_count = param.p_new_swapchain_data->img_count,
            .p_framebuffers = &(param.p_new_swapchain_data->framebuffers),
        };

        clear_framebuffers(alloc_callbacks, clear_fb, err_code);

        ClearSwapchainParam clear_sc = {
            .device = param.device,
            .p_swapchain_data = param.p_new_swapchain_data,
        };
        clear_swapchain(alloc_callbacks, clear_sc, 0);
        *(param.p_new_swapchain_data) = *(param.p_old_swapchain_data);
        *(param.p_old_swapchain_data) = (struct SwapchainEntities){ 0 };
        return RECREATE_SWAPCHAIN_CREATE_FRAMEBUFFER_ERR;
    }

    return RECREATE_SWAPCHAIN_OK;
}
