#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

struct ExcessData {
	int deltas;
	UINT_PTR timer_id;

};

struct WindowData {
	struct ExcessData ex_data;

	HANDLE win_handle;
	int width;
	int height;
	VkPhysicalDevice phy_device;

	VkQueue graphics_queue;
	uint32_t graphics_inx;
	VkQueue present_queue;
	uint32_t present_inx;

	VkSurfaceKHR surface;
	VkDevice device;
	
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

	VkRenderPass render_pass;
	VkPipelineLayout graphics_pipeline_layout;
	VkPipeline graphics_pipeline;

	VkCommandPool cmd_pool;

	UINT curr_frame_in_flight;
	UINT max_frames_in_flight;
	VkCommandBuffer *cmd_buffer;
	VkFence *in_flight_fences;
	VkSemaphore *image_available_semaphores;
	VkSemaphore *render_finished_semaphores;

	VkBuffer vert_buff;
	VkDeviceMemory vert_memory;

	//Temporary members
	VkDescriptorSetLayout descriptor_layout;
	VkBuffer *uniform_buffer;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet* descriptor_sets;

};

typedef struct WindowData WindowData;

VkInstance g_vk_instance = NULL;

int create_instance() {
	VkResult result = VK_SUCCESS;
	int out_res = 0;

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Application",
		.applicationVersion = VK_MAKE_VERSION(1,0,0),
		.pEngineName = "Null Engine",
		.engineVersion = VK_MAKE_VERSION(1,0,0),
		.apiVersion = VK_API_VERSION_1_0
	};


	//Setup instance layers , + check for availability
	const char* instance_layers[] = {
#ifndef NDEBUG
	"VK_LAYER_KHRONOS_validation",
#endif
	""
	};

	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);

	VkLayerProperties* available_layers = malloc(layer_count * sizeof * available_layers);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
	int found = 1;
	for (int i = 0; i < COUNT_OF(instance_layers) - 1; ++i) {
		size_t j;
		for (j = 0; j < layer_count; ++j) {
			if (strcmp(instance_layers[i], available_layers[j].layerName) == 0)
				break;
		}
		if (j == layer_count) {
			found = 0;
			break;
		}
	}
	free(available_layers);
	out_res--;
	if (!found)
		return out_res;

	//Setup instance extensions {opt, check for availability}
	uint32_t win32_extension_count = 0;
	//requires vk khr surface , vk khr win32 surface
	const char* instance_extensions[] = {
#ifndef NDEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		""
	};
	win32_extension_count = COUNT_OF(instance_extensions) - 1;


	uint32_t extension_count;
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);

	VkExtensionProperties* available_extensions = malloc(extension_count * sizeof * available_extensions);
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);
	found = 1;
	for (int i = 0; i < COUNT_OF(instance_extensions) - 1; ++i) {
		int j;
		for (j = 0; j < extension_count; ++j) {
			if (strcmp(instance_extensions[i], available_extensions[j].extensionName) == 0)
				break;
		}
		if (j == extension_count) {
			found = 0;
			break;
		}
	}
	free(available_extensions);
	out_res--;
	if (!found)
		return out_res;


	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = win32_extension_count,
		.ppEnabledExtensionNames = instance_extensions,
		.enabledLayerCount = COUNT_OF(instance_layers) -1,
		.ppEnabledLayerNames = instance_layers
	};


	result = vkCreateInstance(&create_info, NULL, &g_vk_instance);

	out_res--;
	if (result != VK_SUCCESS)
		return out_res;




	return 0;
}


struct SwapChainSupport {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* present_modes;
	uint32_t formats_count;
	uint32_t present_modes_count;
};

int create_device(WindowData* p_win, struct SwapChainSupport* p_swapchain_details) {
	int res = 0;
	VkResult result = VK_SUCCESS;

	p_win->phy_device = VK_NULL_HANDLE;

	uint32_t phy_device_count = 0;
	vkEnumeratePhysicalDevices(g_vk_instance, &phy_device_count, NULL);
	
	res--;
	if (phy_device_count == 0)
		return res;

	VkPhysicalDevice* phy_devices = malloc(phy_device_count * sizeof * phy_devices);
	
	res--;
	if (!phy_devices)
		return res;

	//Required list of device extensions
	const char* req_dev_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		""
	};
	const uint32_t req_dev_extension_count =
		COUNT_OF(req_dev_extensions)- 1;

	//Required list of device layers
	const char* req_dev_layers[] = {
#ifndef NDEBUG
	"VK_LAYER_KHRONOS_validation",
#endif
		""
	};
	const uint32_t req_dev_layers_count = COUNT_OF(req_dev_layers) - 1;

	vkEnumeratePhysicalDevices(g_vk_instance, &phy_device_count, phy_devices);


	//Inside, will test and fill all required queue families

	uint32_t graphics_family;
	uint32_t present_family;
	for (int i = 0; i < phy_device_count; ++i) {

		//test if suitable

		//Check if device extensions are supported
		uint32_t extension_count;
		vkEnumerateDeviceExtensionProperties(phy_devices[i], NULL, &extension_count, NULL);

		VkExtensionProperties* available_extensions = malloc(extension_count * sizeof * available_extensions);
		vkEnumerateDeviceExtensionProperties(phy_devices[i], NULL, &extension_count, available_extensions);

		int found = 1;
		for (int i = 0; i < COUNT_OF(req_dev_extensions) - 1; ++i) {
			int j;
			for (j = 0; j < extension_count; ++j) {
				if (strcmp(req_dev_extensions[i], available_extensions[j].extensionName) == 0)
					break;
			}
			if (j == extension_count) {
				found = 0;
				break;
			}
		}
		free(available_extensions);
		if (!found)
			continue;


		//Check if device layers are supported
		uint32_t layers_count;
		vkEnumerateDeviceLayerProperties(phy_devices[i], &layers_count, NULL);

		VkLayerProperties* available_layers = malloc(extension_count * sizeof * available_layers);
		vkEnumerateDeviceLayerProperties(phy_devices[i], &layers_count, available_layers);

		found = 1;
		for (int i = 0; i < req_dev_layers_count; ++i) {
			int j;
			for (j = 0; j < layers_count; ++j) {
				if (strcmp(req_dev_layers[i], available_layers[j].layerName) == 0)
					break;
			}
			if (j == layers_count) {
				found = 0;
				break;
			}
		}
		free(available_layers);
		if (!found)
			continue;

		//Check device properties and features 
		VkPhysicalDeviceProperties device_props;
		vkGetPhysicalDeviceProperties(phy_devices[i], &device_props);

		VkPhysicalDeviceFeatures device_feats;
		vkGetPhysicalDeviceFeatures(phy_devices[i], &device_feats);


		//Here test for non queue requirements
		if (!device_feats.geometryShader)
			continue;

		//Check for availability of required queue family / rate here

		uint32_t family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[i], &family_count, NULL);
		VkQueueFamilyProperties* families = malloc(family_count * sizeof * families);
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[i], &family_count, families);
		//One by one check for suitability
		BOOL graphics_avail = 0;
		BOOL present_avail = 0;


		for (int jj = 0; jj < family_count; ++jj) {
			int j = family_count - jj - 1;
			j = jj;

			VkBool32 present_capable;
			vkGetPhysicalDeviceSurfaceSupportKHR(phy_devices[i], j, p_win->surface, &present_capable);
			if ((families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT && !graphics_avail)) {
				graphics_family = j;
				graphics_avail = 1;
			}

			if (present_capable && !present_avail) {
				present_family = j;
				present_avail = 1;
			}

			//break if all is available
			if (graphics_avail && present_avail)
				break;

		}

		free(families);

		//Check if all is filled
		if (!graphics_avail || !present_avail)
			continue;

		//Query for the swap_support of swapchain and window surface
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			phy_devices[i],
			p_win->surface,
			&(p_swapchain_details->capabilities));

		vkGetPhysicalDeviceSurfaceFormatsKHR(
			phy_devices[i], p_win->surface, &(p_swapchain_details->formats_count), NULL
		);
		if (!p_swapchain_details->formats_count)
			continue;
		p_swapchain_details->formats = malloc(
			p_swapchain_details->formats_count * sizeof(VkSurfaceFormatKHR)
		);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			phy_devices[i], p_win->surface,
			&(p_swapchain_details->formats_count),
			p_swapchain_details->formats
		);

		vkGetPhysicalDeviceSurfacePresentModesKHR(
			phy_devices[i], p_win->surface, &(p_swapchain_details->present_modes_count), NULL
		);
		if (!p_swapchain_details->present_modes_count) {
			free(p_swapchain_details->formats);
			continue;
		}
		p_swapchain_details->present_modes = malloc(
			p_swapchain_details->present_modes_count * sizeof(VkPresentModeKHR)
		);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			phy_devices[i], p_win->surface,
			&(p_swapchain_details->present_modes_count),
			p_swapchain_details->present_modes
		);

		p_win->phy_device = phy_devices[i];
		break;

	}

	free(phy_devices);

	res--;
	if (p_win->phy_device == VK_NULL_HANDLE)
		return res;

	float queue_priorities = 1.f;
	VkDeviceQueueCreateInfo queue_create_infos[2];
	uint32_t queue_create_count = 1;
	queue_create_infos[0] = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueCount = 1,
		.queueFamilyIndex = graphics_family,
		.pQueuePriorities = &queue_priorities
	};
	//Make a nested if for this case, it's handleable anyways
	if (present_family != graphics_family) {
		queue_create_count++;
		queue_create_infos[1] = queue_create_infos[0];
		queue_create_infos[1].queueFamilyIndex = present_family;
	}

	VkPhysicalDeviceFeatures device_features = { 0 };

	//When required, create the device features, layers check and enable code here

	VkDeviceCreateInfo dev_create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pQueueCreateInfos = queue_create_infos,
		.queueCreateInfoCount = queue_create_count,
		.pEnabledFeatures = &device_features,
		.enabledExtensionCount = req_dev_extension_count,
		.ppEnabledExtensionNames = req_dev_extensions,
		.enabledLayerCount = req_dev_layers_count,
		.ppEnabledLayerNames = req_dev_layers
	};

	result = vkCreateDevice(p_win->phy_device, &dev_create_info, NULL, &(p_win->device));

	res--;
	if (result != VK_SUCCESS)
		return res;

	vkGetDeviceQueue(p_win->device, graphics_family, 0, &(p_win->graphics_queue));
	vkGetDeviceQueue(p_win->device, present_family, 0, &(p_win->present_queue));

	p_win->graphics_inx = graphics_family;
	p_win->present_inx = present_family;

	return 0;
}

int choose_swapchain_details(WindowData* p_win, struct SwapChainSupport* p_swapchain_details) {
	int res = 0;
	VkResult result = VK_SUCCESS;

	//Choose the settings
	//Choose best if availabe else the first format
	p_win->img_format = p_swapchain_details->formats[0];
	for (uint32_t i = 0; i < p_swapchain_details->formats_count; ++i) {
		if (p_swapchain_details->formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
			p_swapchain_details->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			p_win->img_format = p_swapchain_details->formats[i];
			break;
		}
	}
	free(p_swapchain_details->formats);

	//Choose present mode
	p_win->img_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (uint32_t i = 0; i < p_swapchain_details->present_modes_count; ++i) {
		if (p_swapchain_details->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			p_win->img_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	}
	free(p_swapchain_details->present_modes);


	//Choose a img count
	p_win->min_img_count = p_swapchain_details->capabilities.minImageCount + 3;
	if (p_swapchain_details->capabilities.maxImageCount)
		p_win->min_img_count = min(p_win->min_img_count, p_swapchain_details->capabilities.maxImageCount);

	p_win->img_surface_transform_flag = p_swapchain_details->capabilities.currentTransform;


	return res;
}

int create_swapchain(WindowData* p_win) {
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

	

	result = vkCreateSwapchainKHR(p_win->device, &create_info, NULL, &(p_win->curr_swapchain.swapchain));

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

int create_image_views(WindowData* p_win) {
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

		result = vkCreateImageView(p_win->device, &create_info, NULL, p_win->curr_swapchain.img_views + i);
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

uint32_t* read_bytecode_file(const char* file_name, size_t * file_size) {
	FILE* file = fopen(file_name, "rb");

	if (!file) {
		if (file_size)
			*file_size = 0;
		return NULL;
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	
	uint32_t* data = malloc((size + (sizeof(size_t) - 1)) & ~(sizeof(size_t) - 1));
	
	//Assume that data is aligned properly
	if (!data) {
		fclose(file);

		if (file_size)
			*file_size = 0;
		return NULL;
	}

	fread(data, 1, size, file);

	fclose(file);

	if (file_size)
		*file_size = size;
	return data;
}

int create_shader_module(VkDevice device, uint32_t* shader_bytecode, size_t code_size, VkShaderModule* p_shader_module) {
	
	VkResult result = VK_SUCCESS;
	int res = 0;

	res--;
	if (!shader_bytecode)
		return res;

	res--;
	if (!p_shader_module)
		return res;

	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = shader_bytecode,
		.codeSize = code_size
	};

	result = vkCreateShaderModule(device, &create_info, NULL, p_shader_module);

	res--;
	if (result != VK_SUCCESS)
		return res;

	return 0;
}

int create_render_pass(WindowData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	VkAttachmentDescription attachments = {
		.format = p_win->img_format.format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,		//Determines what to do to attachment before render
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,	//Whether to store rendered things back or not
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference attachment_refs = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	//Determines the shader input / output refrenced in the subpass
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
		.pDependencies = & subpass_dependency,
	};

	result = vkCreateRenderPass(p_win->device, &create_info, NULL, &(p_win->render_pass));

	res--;
	if (result != VK_SUCCESS)
		goto create_render_pass;

	res = 0;

	create_render_pass:
	return res;
}

int create_graphics_pipeline(WindowData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	size_t vert_shader_size = 0;
	size_t frag_shader_size = 0;

	uint32_t* vert_shader_code = read_bytecode_file("shaders/out/demo.vert.spv", &vert_shader_size);

	res--;
	if (!vert_shader_code)
		return res;

	uint32_t* frag_shader_code = read_bytecode_file("shaders/out/demo.frag.spv", &frag_shader_size);

	res--;
	if (!frag_shader_code) {
		free(vert_shader_code);
		return res;
	}

	VkShaderModule vert_shader, frag_shader;

	res--;
	if (create_shader_module(p_win->device, vert_shader_code, vert_shader_size, &vert_shader) < 0) {
		free(vert_shader_code);
		free(frag_shader_code);
		return res;
	}
	free(vert_shader_code);

	res--;
	if (create_shader_module(p_win->device, frag_shader_code, frag_shader_size, &frag_shader) < 0) {
		vkDestroyShaderModule(p_win->device, vert_shader, NULL);
		free(frag_shader_code);
		return res;
	}
	free(frag_shader_code);

	VkPipelineShaderStageCreateInfo vert_shader_stage = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert_shader,
		.pName = "main"
	};

	VkPipelineShaderStageCreateInfo frag_shader_stage = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag_shader,
		.pName = "main"
	};

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		vert_shader_stage,frag_shader_stage
	};

	VkVertexInputAttributeDescription vert_input_attr = {
		.offset = 0,
		.binding = 0,
		.location = 0,
		.format = VK_FORMAT_R32G32_SFLOAT
	};

	VkVertexInputBindingDescription vert_input_bind = {
		.binding = 0,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		.stride = sizeof(float) * 2
	};


	VkPipelineVertexInputStateCreateInfo vert_input_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &vert_input_attr,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vert_input_bind,
	};

	VkPipelineInputAssemblyStateCreateInfo input_assem_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};

	VkViewport viewport = {
		.width = (float)p_win->img_swap_extent.width,
		.height = (float)p_win->img_swap_extent.height,
		.maxDepth = 0.f,
		.maxDepth = 1.f
	};

	VkRect2D scissor = {
		.extent = p_win->img_swap_extent
	};


	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = COUNT_OF(dynamic_states),
		.pDynamicStates = dynamic_states
	};

	VkPipelineViewportStateCreateInfo viewport_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pViewports = &viewport,
		.viewportCount = 1,
		.pScissors = &scissor,
		.scissorCount = 1
	};

	VkPipelineRasterizationStateCreateInfo rasterizer_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.f,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE
	};

	VkPipelineMultisampleStateCreateInfo multisample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	//Place for depth/stencil testing 


	//Below differs for multiple frameuffers
	VkPipelineColorBlendAttachmentState color_blend_attach = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
							VK_COLOR_COMPONENT_G_BIT |
							VK_COLOR_COMPONENT_B_BIT |
							VK_COLOR_COMPONENT_A_BIT,
		.blendEnable = VK_FALSE
	};

	VkPipelineColorBlendStateCreateInfo color_blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.pAttachments = &color_blend_attach,
		.attachmentCount = 1
	};

	VkPushConstantRange push_const_range = {
		.size = sizeof(float),
		.offset = 0,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	};


	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_const_range,
		.setLayoutCount = 1,
		.pSetLayouts = &p_win->descriptor_layout
	};

	result = vkCreatePipelineLayout(p_win->device, &layout_info, NULL, &(p_win->graphics_pipeline_layout));
	res--;
	if (result != VK_SUCCESS)
		goto layout_info;

	VkGraphicsPipelineCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = COUNT_OF(shader_stages),
		.pStages = shader_stages,

		.pVertexInputState = &vert_input_info,
		.pInputAssemblyState = &input_assem_info,
		.pViewportState = &viewport_state_info,
		.pRasterizationState = &rasterizer_info,
		.pMultisampleState = &multisample_info,
		.pDepthStencilState = NULL,
		.pColorBlendState = &color_blend_info,
		.pDynamicState = &dynamic_state_info,

		.layout = p_win->graphics_pipeline_layout,

		//This doesn't mean this pipeline should be used with this particular render pass
		//, can be used with any compatible render pass
		.renderPass = p_win->render_pass,
		.subpass = 0,

	};

	result = vkCreateGraphicsPipelines(p_win->device, VK_NULL_HANDLE,
		1, &create_info, NULL, &(p_win->graphics_pipeline));

	res--;
	if (result != VK_SUCCESS)
		goto create_graphics_pipeline;

	res = 0;

	create_graphics_pipeline:
	
	layout_info:
	vkDestroyShaderModule(p_win->device, vert_shader, NULL);
	vkDestroyShaderModule(p_win->device, frag_shader, NULL);

	return res;
}

int create_framebuffers(WindowData* p_win) {
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
		result = vkCreateFramebuffer(p_win->device, &create_info, NULL, &(p_win->curr_swapchain.framebuffers[i]));
		res--;
		if (result != VK_SUCCESS)
			goto create_framebuffers;
	}

	res = 0;
	create_framebuffers:
	alloc_framebuffers:

	return res;
}

int create_command_pool(WindowData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	VkCommandPoolCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = p_win->graphics_inx,
	};
	result = vkCreateCommandPool(p_win->device, &create_info, NULL, &(p_win->cmd_pool));
	res--;
	if (result != VK_SUCCESS)
		return res;

	res = 0;

	return res;
}

int create_command_buffers(WindowData* p_win) {

	VkResult result = VK_SUCCESS;
	int res = 0;

	p_win->cmd_buffer = malloc(p_win->max_frames_in_flight * sizeof(VkCommandBuffer));
	res--;
	if (!p_win->cmd_buffer)
		return res;

	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = p_win->cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = p_win->max_frames_in_flight,
	};

	result = vkAllocateCommandBuffers(p_win->device, &alloc_info, p_win->cmd_buffer);
	res--;
	if (result != VK_SUCCESS)
		goto alloc_cmd_buffer;


	res = 0;
alloc_cmd_buffer:
	return res;
}

int create_sync_objects(WindowData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	p_win->in_flight_fences = malloc(p_win->max_frames_in_flight * sizeof(VkFence));
	res--;
	if (!p_win->in_flight_fences)
		goto clear;

	p_win->render_finished_semaphores = malloc(p_win->max_frames_in_flight * sizeof(VkSemaphore));
	res--;
	if (!p_win->render_finished_semaphores)
		goto clear;

	p_win->image_available_semaphores = malloc(p_win->max_frames_in_flight * sizeof(VkSemaphore));
	res--;
	if (!p_win->image_available_semaphores)
		goto clear;

	VkSemaphoreCreateInfo sema_create_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VkFenceCreateInfo fence_create_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (int i = 0; i < p_win->max_frames_in_flight; ++i) {

		result = vkCreateFence(p_win->device, &fence_create_info, NULL, p_win->in_flight_fences + i);
		res--;
		if (result != VK_SUCCESS)
			return res;

		result = vkCreateSemaphore(p_win->device, &sema_create_info, NULL, p_win->image_available_semaphores + i);
		res--;
		if (result != VK_SUCCESS)
			return res;

		result = vkCreateSemaphore(p_win->device, &sema_create_info, NULL, p_win->render_finished_semaphores + i);
		res--;
		if (result != VK_SUCCESS)
			return res;
	}
	res = 0;

clear:
	return res;
}

int create_descriptor_layout(WindowData* p_win) {

	VkResult result = VK_SUCCESS;
	int res = 0;


	VkDescriptorSetLayoutBinding bind = {
		.binding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	VkDescriptorSetLayoutCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &bind,
	};


	result = vkCreateDescriptorSetLayout(p_win->device, &create_info, NULL, &p_win->descriptor_layout);
	res--;
	if (result != VK_SUCCESS)
		return res;


	res = 0;


	return res;

}


int clear_descriptor_layout(WindowData* p_win) {
	vkDestroyDescriptorSetLayout(p_win->device, p_win->descriptor_layout, NULL);
	return 0;
}

int clear_sync_objects(WindowData* p_win) {

	for (int i = 0; i < p_win->max_frames_in_flight; ++i) {
		vkDestroySemaphore(p_win->device, p_win->render_finished_semaphores[i], NULL);
		vkDestroySemaphore(p_win->device, p_win->image_available_semaphores[i], NULL);
		vkDestroyFence(p_win->device, p_win->in_flight_fences[i], NULL);
	}
	free(p_win->in_flight_fences);
	free(p_win->image_available_semaphores);
	free(p_win->render_finished_semaphores);
	
	return 0;
}

int clear_command_buffers(WindowData* p_win) {
	free(p_win->cmd_buffer);
	return 0;
}

int clear_command_pool(WindowData* p_win) {
	vkDestroyCommandPool(p_win->device, p_win->cmd_pool, NULL);
	return 0;
}

int clear_framebuffers(WindowData* p_win, struct SwapchainEntities * entities) {
	for (int i = 0; i < entities->img_count; ++i) {
		vkDestroyFramebuffer(p_win->device, entities->framebuffers[i], NULL);
	}
	free(entities->framebuffers);
	entities->framebuffers = NULL;
	return 0;
}

int clear_pipeline(WindowData* p_win) {
	vkDestroyPipeline(p_win->device, p_win->graphics_pipeline, NULL);
	vkDestroyPipelineLayout(p_win->device, p_win->graphics_pipeline_layout, NULL);

	return 0;
}

int clear_render_pass(WindowData* p_win) {
	vkDestroyRenderPass(p_win->device, p_win->render_pass, NULL);
	return 0;
}

int clear_image_views(WindowData* p_win, struct SwapchainEntities * entities) {

	for (int i = 0; i < entities->img_count; ++i) {
		vkDestroyImageView(p_win->device, entities->img_views[i], NULL);
	}
	free(entities->img_views);
	entities->img_views = NULL;
	return 0;
}

int clear_swapchain(WindowData* p_win, struct SwapchainEntities * entities) {
	free(entities->images);
	entities->images = NULL;
	vkDestroySwapchainKHR(p_win->device, entities->swapchain, NULL);
	entities->images = NULL;
	entities->swapchain = NULL;
	entities->img_count = 0;
	return 0;
}

int clear_device(WindowData * data) {
	vkDestroyDevice(data->device, NULL);
	data->device = data->phy_device = VK_NULL_HANDLE;
	return 0;
}

int clear_instance() {
	vkDestroyInstance(g_vk_instance, NULL);
	g_vk_instance = VK_NULL_HANDLE;
	return 0;
}


int recreate_swapchain(WindowData* p_win) {
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

int record_command_buffers(WindowData* p_win, uint32_t img_inx, uint32_t frame_inx) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	VkCommandBufferBeginInfo cmd_begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,

	};

	result = vkBeginCommandBuffer(p_win->cmd_buffer[frame_inx], &cmd_begin_info);
	res--;
	if (result != VK_SUCCESS)
		return res;

	VkClearValue img_clear_col = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

	VkRenderPassBeginInfo rndr_begin_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = p_win->render_pass,
		.framebuffer = p_win->curr_swapchain.framebuffers[img_inx],
		.renderArea = {
			.offset = {0,0},
			.extent = p_win->img_swap_extent
		},
		.clearValueCount = 1,
		.pClearValues = &img_clear_col,

	};
	vkCmdBeginRenderPass(p_win->cmd_buffer[frame_inx], &rndr_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(p_win->cmd_buffer[frame_inx], VK_PIPELINE_BIND_POINT_GRAPHICS, p_win->graphics_pipeline);

	VkViewport viewport = {
		.x = 0.f,
		.y = 0.f,
		.width = (float)p_win->img_swap_extent.width,
		.height = (float)p_win->img_swap_extent.height,
		.minDepth = 0.f,
		.maxDepth = 1.f,
	};
	vkCmdSetViewport(p_win->cmd_buffer[frame_inx], 0, 1, &viewport);

	VkRect2D scissor = {
		.offset = {0,0},
		.extent = p_win->img_swap_extent,
	};
	vkCmdSetScissor(p_win->cmd_buffer[frame_inx], 0, 1, &scissor);

	float* del = NULL;

	vkMapMemory(p_win->device, p_win->vert_memory, 1024 * frame_inx + 1024, sizeof(float), 0, &del);
	*del = p_win->ex_data.deltas / 256.f;
	vkUnmapMemory(p_win->device, p_win->vert_memory);

	vkCmdBindDescriptorSets(p_win->cmd_buffer[frame_inx],
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		p_win->graphics_pipeline_layout,
		0, 1, p_win->descriptor_sets + frame_inx,
		0, NULL);

	//vkCmdPushConstants(p_win->cmd_buffer[frame_inx],
	//	p_win->graphics_pipeline_layout,
	//	VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float), &del);

	VkDeviceSize offsets = 0;
	vkCmdBindVertexBuffers(p_win->cmd_buffer[frame_inx], 0, 1, &p_win->vert_buff, &offsets);

	vkCmdDraw(p_win->cmd_buffer[frame_inx], 6, 1, 0, 0);
	vkCmdEndRenderPass(p_win->cmd_buffer[frame_inx]);
	result = vkEndCommandBuffer(p_win->cmd_buffer[frame_inx]);
	res--;
	if (result != VK_SUCCESS)
		return res;


	res = 0;

	return res;
}


int draw_frame(WindowData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	result = vkWaitForFences(p_win->device, 1, p_win->in_flight_fences+ p_win->curr_frame_in_flight,
		VK_TRUE, UINT64_MAX);

	res--;
	if (result == VK_ERROR_OUT_OF_DATE_KHR || !p_win->img_swap_extent.width || !p_win->img_swap_extent.height ) {
		if (recreate_swapchain(p_win) < 0)
			return res;
		return 0;
	}



	vkResetFences(p_win->device, 1, p_win->in_flight_fences + p_win->curr_frame_in_flight);

	uint32_t img_inx;
	vkAcquireNextImageKHR(p_win->device, p_win->curr_swapchain.swapchain, UINT64_MAX,
		p_win->image_available_semaphores[p_win->curr_frame_in_flight], VK_NULL_HANDLE, &img_inx);

	vkResetCommandBuffer(p_win->cmd_buffer[p_win->curr_frame_in_flight], 0);
	record_command_buffers(p_win,img_inx , p_win->curr_frame_in_flight);


	VkSemaphore signal_semaphores[] = {
		p_win->render_finished_semaphores[p_win->curr_frame_in_flight]
	};
	VkSemaphore wait_semaphores[] = {
		p_win->image_available_semaphores[p_win->curr_frame_in_flight]
	};
	VkPipelineStageFlags wait_stages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};


	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = COUNT_OF(wait_semaphores),
		.pWaitSemaphores = wait_semaphores,
		.pWaitDstStageMask = wait_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &(p_win->cmd_buffer[p_win->curr_frame_in_flight]),
		.signalSemaphoreCount = COUNT_OF(signal_semaphores),
		.pSignalSemaphores = signal_semaphores
	};

	result = vkQueueSubmit(p_win->graphics_queue, 1, &submit_info,
		p_win->in_flight_fences[p_win->curr_frame_in_flight]);
	res--;
	if (result != VK_SUCCESS)
		return res;

	VkSwapchainKHR swapchains[] = {
		p_win->curr_swapchain.swapchain
	};

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = COUNT_OF(signal_semaphores),
		.pWaitSemaphores = signal_semaphores,
		.swapchainCount = COUNT_OF(swapchains),
		.pSwapchains = swapchains,
		.pImageIndices = &img_inx,
	};

	result = vkQueuePresentKHR(p_win->present_queue, &present_info);
	
	res--;
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
		return res;

	p_win->curr_frame_in_flight = (p_win->curr_frame_in_flight + 1) % p_win->max_frames_in_flight;

	res--;
	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
		if (recreate_swapchain(p_win) < 0)
			return res;

	res = 0;

	return res;
}

LRESULT CALLBACK wnd_proc(HWND h_wnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	WindowData* p_win = NULL;

	if (msg == WM_CREATE) {
		CREATESTRUCT* cr_data = (CREATESTRUCT*)lparam;
		p_win = (WindowData*)cr_data->lpCreateParams;
		SetWindowLongPtr(h_wnd, GWLP_USERDATA, (LONG_PTR)p_win);
		
		*p_win = (WindowData){ 0 };
		p_win->win_handle = h_wnd;

		RECT client_area;
		GetClientRect(h_wnd, &client_area);
		p_win->width = client_area.right;
		p_win->height = client_area.bottom;

		VkWin32SurfaceCreateInfoKHR surface_create_info = {
			.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.hwnd = h_wnd,
			.hinstance = GetModuleHandle(NULL)
		};
		int res = 0;

		p_win->max_frames_in_flight = 2;
		p_win->curr_frame_in_flight = 0;

		res--;
		if (vkCreateWin32SurfaceKHR(g_vk_instance, &surface_create_info, NULL, &(p_win->surface)) != VK_SUCCESS)
			return res;
		struct SwapChainSupport swap_support;
		res--;
		if (create_device(p_win, &swap_support) < 0)
			return res;
		res--;
		if (choose_swapchain_details(p_win, &swap_support) < 0) {
			return  res;
		}
		res--;
		if (create_swapchain(p_win) < 0) {
			return  res;
		}
		res--;
		if (create_image_views(p_win) < 0) {
			return res;
		}
		res--;
		if (create_render_pass(p_win) < 0)
			return res;
		res--;
		if (create_descriptor_layout(p_win) < 0)
			return res;
		res--;
		if (create_graphics_pipeline(p_win) < 0) {
			return res;
		}
		res--;
		if (create_framebuffers(p_win) < 0)
			return res;
		res--;
		if (create_command_pool(p_win) < 0)
			return res;
		res--;
		if (create_command_buffers(p_win) < 0)
			return res;
		res--;
		if (create_sync_objects(p_win) < 0)
			return res;

		//Playground for stuff
		VkResult result = VK_SUCCESS;

		p_win->uniform_buffer = malloc(p_win->max_frames_in_flight * sizeof(VkBuffer));
		res--;
		if (!p_win->uniform_buffer)
			return res;

		VkPhysicalDeviceMemoryProperties mem_props;

		vkGetPhysicalDeviceMemoryProperties(p_win->phy_device, &mem_props);

		VkBufferCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = 1024,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};

		result = vkCreateBuffer(p_win->device, &info, NULL, &p_win->vert_buff);

		res--;
		if (result != VK_SUCCESS)
			return res;

		VkMemoryRequirements buff_mem_req1;

		vkGetBufferMemoryRequirements(p_win->device, p_win->vert_buff, &buff_mem_req1);
		
		info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		uint32_t reqs = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		for (int i = 0; i < p_win->max_frames_in_flight; ++i) {

			result = vkCreateBuffer(p_win->device, &info, NULL, &p_win->uniform_buffer[i]);

			res--;
			if (result != VK_SUCCESS)
				return res;
		}

		VkMemoryRequirements buff_mem_req2;

		vkGetBufferMemoryRequirements(p_win->device, p_win->uniform_buffer[0], &buff_mem_req2);


		int32_t mem_inx = -1;
		for (int i = 0; i < mem_props.memoryTypeCount; ++i) {
			if (((1 << i) & buff_mem_req1.memoryTypeBits) &&
				((1 << i) & buff_mem_req2.memoryTypeBits) && 
				(reqs == (reqs & mem_props.memoryTypes[i].propertyFlags))) {
				mem_inx = i;
				break;
			}
		}

		VkMemoryAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = 1024 + 1024 * p_win->max_frames_in_flight,
			.memoryTypeIndex = mem_inx,
		};

		result = vkAllocateMemory(p_win->device, &alloc_info, NULL, &p_win->vert_memory);

		res--;
		if (result != VK_SUCCESS)
			return res;

		for (int i = 0; i < p_win->max_frames_in_flight; ++i) {
			vkBindBufferMemory(p_win->device, p_win->uniform_buffer[i], p_win->vert_memory, 1024 * (i + 1));
		}

		vkBindBufferMemory(p_win->device, p_win->vert_buff, p_win->vert_memory, 0);


		float* ptr;
		vkMapMemory(p_win->device, p_win->vert_memory, 0, 1024, 0, &ptr);
		float memdat[] = { -0.5, -0.5,0.5, 0.5,-0.5, 0.5 ,
		0.5, 0.5,-0.5, -0.5,0.5, -0.5 };
		memcpy(ptr, memdat, sizeof(memdat));
		vkUnmapMemory(p_win->device, p_win->vert_memory);


		VkDescriptorPoolSize descriptor_pool_size = {
			.descriptorCount = p_win->max_frames_in_flight,
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		};

		VkDescriptorPoolCreateInfo descriptor_pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = p_win->max_frames_in_flight,
			.poolSizeCount = 1,
			.pPoolSizes = &descriptor_pool_size,
		};

		result = vkCreateDescriptorPool(p_win->device, &descriptor_pool_info, NULL, &p_win->descriptor_pool);

		res--;
		if (result != VK_SUCCESS) {
			return res;
		}

		p_win->descriptor_sets = malloc(p_win->max_frames_in_flight * sizeof(VkDescriptorSet));

		res--;
		if (!p_win->descriptor_sets)
			return res;

		for (int i = 0; i < p_win->max_frames_in_flight; ++i) {
			VkDescriptorSetAllocateInfo descriptor_sets_alloc = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorSetCount = 1,
				.pSetLayouts = &p_win->descriptor_layout,
				.descriptorPool = p_win->descriptor_pool,
			};

			result = vkAllocateDescriptorSets(p_win->device,
				&descriptor_sets_alloc,
				p_win->descriptor_sets + i);

			res--;
			if (result != VK_SUCCESS)
				return res;
						
		}

		VkWriteDescriptorSet* descriptor_write_structs = malloc(
			p_win->max_frames_in_flight * (
				sizeof(VkWriteDescriptorSet) +
				sizeof(VkDescriptorBufferInfo))
		);

		res--;
		if (!descriptor_write_structs)
			return res;

		VkDescriptorBufferInfo* descriptor_buff_infos = (VkDescriptorBufferInfo*)
			(descriptor_write_structs +
			p_win->max_frames_in_flight * sizeof(VkWriteDescriptorSet));

		for (int i = 0; i < p_win->max_frames_in_flight; ++i) {
			descriptor_buff_infos[i] = (VkDescriptorBufferInfo){
				.buffer = p_win->uniform_buffer[i],
				.offset = 0,
				.range = sizeof(float)
			};

			descriptor_write_structs[i] = (VkWriteDescriptorSet){
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.dstArrayElement = 0,
				.dstBinding = 0,
				.dstSet = p_win->descriptor_sets[i],
				.pBufferInfo = descriptor_buff_infos + i,

			};

			
		}

		vkUpdateDescriptorSets(p_win->device,
			p_win->max_frames_in_flight,
			descriptor_write_structs,
			0, NULL);

		free(descriptor_write_structs);

		p_win->ex_data.deltas = 0;
		
		p_win->ex_data.timer_id = SetTimer(h_wnd, 111, 10, NULL);


		

	}
	else {
		p_win = (WindowData*)GetWindowLongPtr(h_wnd, GWLP_USERDATA);
	}

	if (p_win) {

		if (msg == WM_DESTROY) {
			KillTimer(h_wnd, p_win->ex_data.timer_id);
			
			vkDeviceWaitIdle(p_win->device);


			vkDestroyDescriptorPool(p_win->device, p_win->descriptor_pool, NULL);
			clear_descriptor_layout(p_win);

			vkDestroyBuffer(p_win->device, p_win->vert_buff, NULL);
			vkFreeMemory(p_win->device, p_win->vert_memory, NULL);

			for (int i = 0; i < p_win->max_frames_in_flight; ++i) {

				vkDestroyBuffer(p_win->device, p_win->uniform_buffer[i], NULL);
			}
			free(p_win->descriptor_sets);
			free(p_win->uniform_buffer);


			clear_sync_objects(p_win);
			clear_command_buffers(p_win);
			clear_command_pool(p_win);
			p_win->old_swapchain.img_count = p_win->curr_swapchain.img_count;
			if (p_win->old_swapchain.framebuffers)
				clear_framebuffers(p_win, &p_win->old_swapchain);
			clear_framebuffers(p_win,&p_win->curr_swapchain);
			clear_pipeline(p_win);
			clear_render_pass(p_win);
			if (p_win->old_swapchain.img_views)
				clear_image_views(p_win, &p_win->old_swapchain);
			clear_image_views(p_win,&p_win->curr_swapchain);
			if (p_win->old_swapchain.swapchain)
				clear_swapchain(p_win, &p_win->old_swapchain);
			clear_swapchain(p_win,&p_win->curr_swapchain);
			clear_device(p_win);
			vkDestroySurfaceKHR(g_vk_instance, p_win->surface, NULL);
			PostQuitMessage(0);
			return 0;
		}
		if (msg == WM_PAINT) {
			draw_frame(p_win);
			p_win->ex_data.deltas = (p_win->ex_data.deltas + 257) % (2 * 256) - 256;
		}
		if (msg == WM_TIMER) {
			if (wparam == p_win->ex_data.timer_id) {
				InvalidateRect(h_wnd, NULL, TRUE);
			}
		}
		if (msg == WM_SIZE) {

			RECT client_area;
			GetClientRect(h_wnd, &client_area);
			p_win->width = client_area.right;
			p_win->height = client_area.bottom;
			//recreate_swapchain(p_win);
		}
		if (msg == WM_SETCURSOR) {
			HCURSOR cursor = LoadCursor(NULL, IDC_UPARROW);
			SetCursor(cursor);
		}
	}


	return DefWindowProc(h_wnd, msg, wparam, lparam);
}

int main(int argc, char* argv[]) {

	HINSTANCE h_instance = GetModuleHandle(NULL);

	LPWSTR wndclass_name = L"Window Class";

	WNDCLASS wnd_class = { 0 };
	wnd_class.hInstance = h_instance;
	wnd_class.lpszClassName = wndclass_name;
	wnd_class.lpfnWndProc = wnd_proc;
	RegisterClass(&wnd_class);


	if (create_instance() < 0)
		return -1;

	WindowData wnd_data = { 0 };
	if (!CreateWindowEx(0, wndclass_name, L"Window", WS_OVERLAPPEDWINDOW , 20, 10, 600, 600, NULL, NULL, h_instance, &wnd_data))
		return -1;

	ShowWindow(wnd_data.win_handle, SW_SHOW);

	//From here on, assumes none tasks fail


	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	clear_instance();

	UnregisterClass(wnd_class.lpszClassName, h_instance);

	return 0;
}