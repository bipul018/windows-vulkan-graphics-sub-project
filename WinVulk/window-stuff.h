#pragma once



//Data like window handle, size, surface, images, framebuffers, swapchains
struct WindowData {
	struct ExcessData ex_data;
	HANDLE win_handle;
	int width;
	int height;


	VkSurfaceKHR surface;


	uint32_t min_img_count;
	VkSurfaceFormatKHR img_format;
	VkSurfaceTransformFlagBitsKHR img_surface_transform_flag;
	VkPresentModeKHR img_present_mode;
	VkExtent2D img_swap_extent;

	//A struct of everything that depends on swapchain
	struct SwapchainEntities {
		VkSwapchainKHR swapchain;

		//All following have this img_count elements if successfully created
		uint32_t img_count;
		VkImage* images;
		VkImageView* img_views;
		VkFramebuffer* framebuffers;
	} curr_swapchain;

	struct SwapchainEntities old_swapchain;

};

typedef struct WindowData WindowData;





int create_swapchain(GlobalData* p_win) {
	int res = 0;
	VkResult result = VK_SUCCESS;

	VkSurfaceCapabilitiesKHR surf_capa;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		p_win->phy_device,
		p_win->surface,
		&surf_capa);

	//Choose swap extent
	// ++++
	//  TODO:: make dpi aware, ..., maybe not needed
	// ++++
	//Now just use set width and height, also currently not checked anything from capabilities
	//Also be aware of max and min extent set to numeric max of uint32_t
	if (surf_capa.currentExtent.width != -1 && surf_capa.currentExtent.height != -1) {

		p_win->img_swap_extent = surf_capa.currentExtent;

	}
	else {
		p_win->img_swap_extent.width = p_win->width;
		p_win->img_swap_extent.height = p_win->height;

	}

	res--;
	if (!p_win->img_swap_extent.width || !p_win->img_swap_extent.height) {
		return res;
	}

	//An array of queue family indices used
	uint32_t indices_array[] = { p_win->graphics_inx,p_win->present_inx };


	VkSwapchainCreateInfoKHR create_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = p_win->surface,
		.minImageCount = p_win->min_img_count,
		.imageFormat = p_win->img_format.format,
		.imageColorSpace = p_win->img_format.colorSpace,
		.imageExtent = p_win->img_swap_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = p_win->img_surface_transform_flag,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = p_win->img_present_mode,
		.clipped = VK_TRUE,			//This should be false only if pixels have to re read
		.oldSwapchain = p_win->old_swapchain.swapchain,
		.imageSharingMode = VK_SHARING_MODE_CONCURRENT,
		//Here , for exclusive sharing mode it is  optional; else for concurrent, there has to be at
		//least two different queue families, and all should be specified to share the images amoong
		.queueFamilyIndexCount = COUNT_OF(indices_array),
		.pQueueFamilyIndices = indices_array
	};

	if (indices_array[0] == indices_array[1]) {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}



	result = vkCreateSwapchainKHR(p_win->device, &create_info,
		p_win->p_host_alloc_calls,
		&(p_win->curr_swapchain.swapchain));

	res--;
	if (result != VK_SUCCESS)
		return res;

	result = vkGetSwapchainImagesKHR(p_win->device, p_win->curr_swapchain.swapchain,
		&(p_win->curr_swapchain.img_count), NULL);
	res--;
	if (result != VK_SUCCESS)
		return res;
	p_win->curr_swapchain.images = malloc(p_win->curr_swapchain.img_count * sizeof(VkImage));
	vkGetSwapchainImagesKHR(p_win->device, p_win->curr_swapchain.swapchain,
		&(p_win->curr_swapchain.img_count), p_win->curr_swapchain.images);

	return 0;
}

int create_image_views(GlobalData* p_win) {
	int res = 0;
	VkResult result = VK_SUCCESS;

	p_win->curr_swapchain.img_views = malloc(p_win->curr_swapchain.img_count * sizeof(VkImageView));
	for (size_t i = 0; i < p_win->curr_swapchain.img_count; ++i) {
		VkImageViewCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = p_win->curr_swapchain.images[i],

			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = p_win->img_format.format,

			.components = (VkComponentMapping){
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},

			.subresourceRange = (VkImageSubresourceRange){
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}

		};

		result = vkCreateImageView(p_win->device, &create_info,
			p_win->p_host_alloc_calls,
			p_win->curr_swapchain.img_views + i);
		if (result != VK_SUCCESS)
			break;

	}

	res--;
	if (result != VK_SUCCESS) {
		free(p_win->curr_swapchain.img_views);
		p_win->curr_swapchain.img_views = NULL;
		p_win->curr_swapchain.img_count = 0;
		return res;
	}

	return 0;
}




int create_framebuffers(GlobalData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	p_win->curr_swapchain.framebuffers = malloc(p_win->curr_swapchain.img_count * sizeof(VkFramebuffer));

	res--;
	if (!p_win->curr_swapchain.framebuffers)
		goto alloc_framebuffers;

	for (int i = 0; i < p_win->curr_swapchain.img_count; ++i) {
		VkImageView img_attachments[] = {
			p_win->curr_swapchain.img_views[i]
		};
		VkFramebufferCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = p_win->render_pass,
			.attachmentCount = COUNT_OF(img_attachments),
			.pAttachments = img_attachments,
			.width = p_win->img_swap_extent.width,
			.height = p_win->img_swap_extent.height,
			.layers = 1
		};
		result = vkCreateFramebuffer(p_win->device, &create_info,
			p_win->p_host_alloc_calls,
			&(p_win->curr_swapchain.framebuffers[i]));
		res--;
		if (result != VK_SUCCESS)
			goto create_framebuffers;
	}

	res = 0;
create_framebuffers:
alloc_framebuffers:

	return res;
}



int clear_framebuffers(GlobalData* p_win, struct SwapchainEntities* entities) {
	for (int i = 0; i < entities->img_count; ++i) {
		vkDestroyFramebuffer(p_win->device, entities->framebuffers[i],
			p_win->p_host_alloc_calls);
	}
	free(entities->framebuffers);
	entities->framebuffers = NULL;
	return 0;
}




int clear_image_views(GlobalData* p_win, struct SwapchainEntities* entities) {

	for (int i = 0; i < entities->img_count; ++i) {
		vkDestroyImageView(p_win->device, entities->img_views[i],
			p_win->p_host_alloc_calls);
	}
	free(entities->img_views);
	entities->img_views = NULL;
	return 0;
}

int clear_swapchain(GlobalData* p_win, struct SwapchainEntities* entities) {
	free(entities->images);
	entities->images = NULL;
	vkDestroySwapchainKHR(p_win->device, entities->swapchain,
		p_win->p_host_alloc_calls);
	entities->images = NULL;
	entities->swapchain = NULL;
	entities->img_count = 0;
	return 0;
}



int recreate_swapchain(GlobalData* p_win) {
	int res = 0;


	if (p_win->old_swapchain.swapchain) {
		if (p_win->old_swapchain.img_count > 0)
			vkDeviceWaitIdle(p_win->device);
		p_win->old_swapchain.img_count = p_win->curr_swapchain.img_count;
		clear_framebuffers(p_win, &p_win->old_swapchain);
		clear_image_views(p_win, &p_win->old_swapchain);
		clear_swapchain(p_win, &p_win->old_swapchain);
	}

	p_win->old_swapchain = p_win->curr_swapchain;

	res--;
	if (create_swapchain(p_win) < 0) {
		//Return if any error, like zero framebuffer size, with swapchain unchanged
		p_win->curr_swapchain = p_win->old_swapchain;
		p_win->old_swapchain = (struct SwapchainEntities){ 0 };
		return res;
	}

	res--;
	if (create_image_views(p_win) < 0)
		return res;

	res--;
	if (create_framebuffers(p_win) < 0)
		return res;



	return 0;
}