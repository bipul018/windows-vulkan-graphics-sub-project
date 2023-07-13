#pragma once

#include <vulkan/vulkan.h>

//Data like g_instance, logical & physical devices selected, queue families inx, if made, allocators
//Maybe also command buffers, if created in same thread
struct CommonData {
	VkInstance g_vk_instance;

	VkPhysicalDevice phy_device;
	VkAllocationCallbacks* p_host_alloc_calls;

	VkQueue graphics_queue;
	uint32_t graphics_inx;
	VkQueue present_queue;
	uint32_t present_inx;
	VkDevice device;

	VkCommandPool cmd_pool;


	struct GPUAllocation {
		VkDeviceMemory memory_handle;
		void* mapped_memory;
		VkDeviceSize mem_size;
		VkDeviceSize curr_offset;
	} mem_allocation;
};

typedef struct CommonData CommonData;
#include <string.h>
struct VulkanLayer {
	const char* layer_name;
	int required;
	int available;
};

int test_vulkan_layer_presence(struct VulkanLayer* to_test, VkLayerProperties* avail_layers, int avail_count) {
	to_test->available = 0;
	for (int i = 0; i < avail_count; ++i) {
		if (strcmp(to_test->layer_name, avail_layers[i].layerName) == 0) {
			to_test->available = 1;
			break;
		}

	}
	return (to_test->available) || !(to_test->required);
}

	/*const char* instance_layers[] = {
#ifndef NDEBUG
	"VK_LAYER_KHRONOS_validation",
#endif
	""
	};*/

struct VulkanExtension {
	const char* extension_name;
	int required;
	int available;
	struct VulkanLayer* layer;
};

int test_vulkan_extension_presence(struct VulkanExtension* to_test, VkExtensionProperties* avail_extensions, int avail_count) {
	to_test->available = 0;
	for (int i = 0; i < avail_count; ++i) {
		if (strcmp(to_test->extension_name, avail_extensions[i].extensionName) == 0) {
			to_test->available = 1;
			break;
		}

	}
	return (to_test->available) || !(to_test->required);
}

//requires vk khr surface , vk khr win32 surface
//const char* instance_extensions[] = {
//#ifndef NDEBUG
//		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
//#endif
//		VK_KHR_SURFACE_EXTENSION_NAME,
//		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
//		""
//};

#include <stdlib.h>

struct StackAllocator {
	uint8_t* base_memory;
	size_t reserved_memory;
	size_t committed_memory;
	size_t page_size;
};
typedef struct StackAllocator StackAllocator;

#define align_up_(size, alignment) (((size) + (alignment) - 1ULL) & ~((alignment)- 1ULL))
#define SIZE_KB(size) (size<<10ULL)
#define SIZE_MB(size) (size<<20ULL)
#define SIZE_GB(size) (size<<30ULL)

#include <memoryapi.h>
StackAllocator alloc_stack_allocator(size_t reserve, size_t commit) {
	StackAllocator allocr = { 0 };
	allocr.page_size = SIZE_KB(4);

	SYSTEM_INFO sys_info = { 0 };
	GetSystemInfo(&sys_info);
	allocr.page_size = align_up_(allocr.page_size, sys_info.dwPageSize);

	reserve = align_up_(reserve, allocr.page_size);
	commit = align_up_(commit, allocr.page_size);
	allocr.base_memory = VirtualAlloc(NULL, reserve, MEM_RESERVE, PAGE_READWRITE);
	if (!allocr.base_memory)
		return allocr;
	allocr.reserved_memory = reserve;
	if (NULL == VirtualAlloc(allocr.base_memory, commit, MEM_COMMIT, PAGE_READWRITE))
		return allocr;
	allocr.committed_memory = commit;
	return allocr;
}

void dealloc_stack_allocator(StackAllocator* allocr) {
	if (allocr)
		VirtualFree(allocr->base_memory, 0, MEM_RELEASE);
}

//alignment should be a power of 2
void* stack_allocate(StackAllocator* data,size_t* curr_offset, size_t size, size_t alignment) {

	if (!data || !size || !curr_offset)
		return NULL;

	uintptr_t next_mem = ((uintptr_t)data->base_memory) + *curr_offset;

	next_mem = align_up_(next_mem, alignment);

	size_t next_offset = next_mem - (uintptr_t)data->base_memory;

	if (next_offset > data->reserved_memory)
		return NULL;

	if (next_mem > data->committed_memory) {
		VirtualAlloc(data->committed_memory + (uintptr_t)data->base_memory,
			align_up_(next_offset - data->committed_memory, data->page_size), MEM_COMMIT, PAGE_READWRITE);
		data->committed_memory += align_up_(next_offset - data->committed_memory, data->page_size);

	}

	*curr_offset = next_offset;
	return next_mem;

}


enum CreateInstanceCodes {
	CREATE_INSTANCE_OK,
	CREATE_INSTANCE_FAILED,
	CREATE_INSTANCE_EXTENSION_NOT_FOUND,
	CREATE_INSTANCE_EXTENSION_CHECK_ALLOC_ERROR,
	CREATE_INSTANCE_LAYER_NOT_FOUND,
	CREATE_INSTANCE_LAYER_CHECK_ALLOC_ERROR,
};


int create_instance(StackAllocator* stk_allocr, size_t stk_allocr_offset,
	VkAllocationCallbacks* alloc_callbacks,
	VkInstance* p_vk_instance, 
	struct VulkanLayer* instance_layers, size_t layers_count,
	struct VulkanExtension* instance_extensions, size_t extensions_count
) {
	VkResult result = VK_SUCCESS;

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Application",
		.applicationVersion = VK_MAKE_VERSION(1,0,0),
		.pEngineName = "Null Engine",
		.engineVersion = VK_MAKE_VERSION(1,0,0),
		.apiVersion = VK_API_VERSION_1_0
	};


	//Setup instance layers , + check for availability

	uint32_t avail_layer_count;
	vkEnumerateInstanceLayerProperties(&avail_layer_count, NULL);

	//VkLayerProperties* available_layers = malloc(avail_layer_count * sizeof * available_layers);
	VkLayerProperties* available_layers = stack_allocate(
		stk_allocr, &stk_allocr_offset, avail_layer_count * sizeof * available_layers, 0);
	
	if (!available_layers)
		return CREATE_INSTANCE_LAYER_CHECK_ALLOC_ERROR;

	vkEnumerateInstanceLayerProperties(&avail_layer_count, available_layers);
	int found = 1;
	for (int i = 0; i < layers_count; ++i) {
		found = found && test_vulkan_layer_presence(instance_layers + i, available_layers, avail_layer_count);
		if (!instance_layers[i].available) {
			struct VulkanLayer layer = instance_layers[i];
			instance_layers[i--] = instance_layers[layers_count - 1];
			instance_layers[--layers_count] = layer;
		}
	}
	//free(available_layers);

	if (!found)
		return CREATE_INSTANCE_LAYER_NOT_FOUND;

	//Setup instance extensions {opt, check for availability}

	uint32_t avail_extension_count;
	vkEnumerateInstanceExtensionProperties(NULL, &avail_extension_count, NULL);

	//VkExtensionProperties* available_extensions = malloc(avail_extension_count * sizeof * available_extensions);
	VkExtensionProperties* available_extensions = stack_allocate(stk_allocr, &stk_allocr_offset,
		(avail_extension_count * sizeof * available_extensions), 0);
	if (!available_extensions)
		return CREATE_INSTANCE_EXTENSION_CHECK_ALLOC_ERROR;

	vkEnumerateInstanceExtensionProperties(NULL, &avail_extension_count, available_extensions);
	found = 1;
	for (int i = 0; i < extensions_count; ++i) {
		found = found && test_vulkan_extension_presence(instance_extensions + i, available_extensions, avail_extension_count);
		if (!instance_extensions[i].available) {
			struct VulkanExtension extension = instance_extensions[i];
			instance_extensions[i--] = instance_extensions[extensions_count - 1];
			instance_extensions[--extensions_count] = extension;
		}
	}
	//free(available_extensions);

	if (!found)
		return CREATE_INSTANCE_EXTENSION_NOT_FOUND;


	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = extensions_count,
		.ppEnabledExtensionNames = (const char**)instance_extensions,
		.enabledLayerCount = layers_count,
		.ppEnabledLayerNames = (const char**)instance_layers,
	};


	result = vkCreateInstance(&create_info, alloc_callbacks, p_vk_instance);

	if (result != VK_SUCCESS)
		return CREATE_INSTANCE_FAILED;

	return CREATE_INSTANCE_OK;
}


struct SwapChainSupport {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* present_modes;
	uint32_t formats_count;
	uint32_t present_modes_count;
};


struct VulkanDevice {
	VkPhysicalDevice phy_device;
	VkDevice device;

	VkQueue graphics_queue;
	uint32_t graphics_family_inx;

	VkQueue present_queue;
	uint32_t present_family_inx;
};

//const char* req_dev_layers[] = {
//#ifndef NDEBUG
//	"VK_LAYER_KHRONOS_validation",
//#endif
//		""
//};

//const char* req_dev_extensions[] = {
//	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
//	""
//};
enum CreateDeviceCodes {
	CREATE_DEVICE_OK,
	CREATE_DEVICE_NO_GPU,
	CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL,
	CREATE_DEVICE_NO_SUITABLE_GPU,
	CREATE_DEVICE_FAILED,

};

typedef struct {
	struct VulkanDevice* p_vk_device;
	VkInstance vk_instance;
	VkSurfaceKHR chosen_surface;
	struct SwapChainSupport* p_swapchain_details;
} CreateDeviceParam;

int create_device(StackAllocator* stk_allocr, size_t stk_allocr_offset,
	VkAllocationCallbacks* alloc_callbacks,
	CreateDeviceParam param,
	struct VulkanLayer *device_layers, size_t layers_count,
	struct VulkanExtension *device_extensions, size_t extensions_count) {
	

	VkResult result = VK_SUCCESS;

	param.p_vk_device->phy_device = VK_NULL_HANDLE;

	uint32_t phy_device_count = 0;
	vkEnumeratePhysicalDevices(param.vk_instance, &phy_device_count, NULL);

	
	if (phy_device_count == 0)
		return CREATE_DEVICE_NO_GPU;

	VkPhysicalDevice* phy_devices = stack_allocate(stk_allocr, &stk_allocr_offset,
		(phy_device_count * sizeof * phy_devices), 0);

	if (!phy_devices)
		return CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL;


	vkEnumeratePhysicalDevices(param.vk_instance, &phy_device_count, phy_devices);


	//Inside, will test and fill all required queue families

	uint32_t graphics_family;
	uint32_t present_family;
	for (int i = 0; i < phy_device_count; ++i) {

		size_t local_stk = stk_allocr_offset;

		//test if suitable

		//Check if device extensions are supported
		uint32_t avail_extension_count;
		vkEnumerateDeviceExtensionProperties(phy_devices[i], NULL, &avail_extension_count, NULL);

		VkExtensionProperties* available_extensions = stack_allocate(stk_allocr, &local_stk,
			(avail_extension_count * sizeof * available_extensions), 0);

		if (!available_extensions)
			return CREATE_DEVICE_PHY_DEVICE_TEST_ALLOC_FAIL;

		vkEnumerateDeviceExtensionProperties(phy_devices[i], NULL, &avail_extension_count, available_extensions);

		int found = 1;
		for (int i = 0; i < extensions_count; ++i) {
			found = found && test_vulkan_extension_presence(device_extensions + i, available_extensions, avail_extension_count);
		}
		if (!found)
			continue;


		//Check if device layers are supported
		uint32_t avail_layers_count;
		vkEnumerateDeviceLayerProperties(phy_devices[i], &avail_layers_count, NULL);

		VkLayerProperties* available_layers = stack_allocate(stk_allocr, &local_stk,
			(avail_layers_count * sizeof * available_layers), 0);

		vkEnumerateDeviceLayerProperties(phy_devices[i], &avail_layers_count, available_layers);

		found = 1;
		for (int i = 0; i < layers_count; ++i) {
			found = found && test_vulkan_layer_presence(device_layers + i, available_extensions, avail_layers_count);
		}
		if (!found)
			continue;

		//Check device properties and features 
		VkPhysicalDeviceProperties device_props;
		vkGetPhysicalDeviceProperties(phy_devices[i], &device_props);

		VkPhysicalDeviceFeatures device_feats;
		vkGetPhysicalDeviceFeatures(phy_devices[i], &device_feats);


		//Here test for non queue requirements
		//if (!device_feats.geometryShader)
		//	continue;

		//Check for availability of required queue family / rate here

		uint32_t family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[i], &family_count, NULL);
		VkQueueFamilyProperties* families = stack_allocate(stk_allocr, &local_stk,
			(family_count * sizeof * families), 0);

		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[i], &family_count, families);
		//One by one check for suitability
		BOOL graphics_avail = 0;
		BOOL present_avail = 0;


		for (int jj = 0; jj < family_count; ++jj) {
			int j = family_count - jj - 1;
			j = jj;

			VkBool32 present_capable;
			vkGetPhysicalDeviceSurfaceSupportKHR(phy_devices[i], j, param.chosen_surface, &present_capable);
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


		//Check if all is filled
		if (!graphics_avail || !present_avail)
			continue;

		//Choose swapchain details

		param.p_vk_device->phy_device = phy_devices[i];
		break;

	}

	//free(phy_devices);

	
	if (param.p_vk_device->phy_device == VK_NULL_HANDLE)
		return CREATE_DEVICE_NO_SUITABLE_GPU;

	//Rearrange the layers and extensions
	for (int i = 0; i < layers_count; ++i) {
		if (!device_layers[i].available) {
			struct VulkanLayer layer = device_layers[i];
			device_layers[i--] = device_layers[layers_count - 1];
			device_layers[--layers_count] = layer;
		}
	}

	for (int i = 0; i < extensions_count; ++i) {
		if (!device_extensions[i].available) {
			struct VulkanExtension extension = device_extensions[i];
			device_extensions[i--] = device_extensions[extensions_count - 1];
			device_extensions[--extensions_count] = extension;
		}
	}



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
		.enabledExtensionCount = extensions_count,
		.ppEnabledExtensionNames = device_extensions,
		.enabledLayerCount = layers_count,
		.ppEnabledLayerNames = device_layers,
	};

	result = vkCreateDevice(param.p_vk_device->phy_device, &dev_create_info,
		alloc_callbacks,
		&param.p_vk_device->device);

	if (result != VK_SUCCESS)
		return CREATE_DEVICE_FAILED;

	vkGetDeviceQueue(param.p_vk_device->device, graphics_family, 0, &param.p_vk_device->graphics_queue);
	vkGetDeviceQueue(param.p_vk_device->device, present_family, 0, &param.p_vk_device->present_queue);

	param.p_vk_device->graphics_family_inx = graphics_family;
	param.p_vk_device->present_family_inx = present_family;

	return 0;
}

typedef struct  {
	VkSurfaceFormatKHR* img_format;
	VkPresentModeKHR* present_mode;
	uint32_t* min_img_count;
	VkSurfaceTransformFlagBitsKHR* transform_flags;

	VkPhysicalDevice phy_device;
	VkSurfaceKHR surface;
} ChooseSwapchainDetsParam;

int choose_swapchain_details(StackAllocator* stk_allocr, size_t stk_offset,ChooseSwapchainDetsParam param ) {
	int res = 0;
	VkResult result = VK_SUCCESS;


	//Query for the swap_support of swapchain and window surface
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		param.phy_device,
		param.surface,
		&capabilities);

	VkSurfaceFormatKHR* surface_formats;
	uint32_t formats_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(
		param.phy_device, param.surface, &formats_count, NULL
	);
	if (!formats_count)
		return -1;

	surface_formats = stack_allocate(stk_allocr, &stk_offset,formats_count * sizeof(VkSurfaceFormatKHR), 0);

	vkGetPhysicalDeviceSurfaceFormatsKHR(
		param.phy_device, param.surface,
		formats_count, surface_formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(
		param.phy_device, param.surface, &param.p_swapchain_details->present_modes_count, NULL
	);
	if (!param.p_swapchain_details->present_modes_count) {
		free(param.p_swapchain_details->formats);
		param.p_swapchain_details->formats = NULL;
		continue;
	}
	if (param.p_swapchain_details->present_modes)
		free(param.p_swapchain_details->present_modes);
	param.p_swapchain_details->present_modes = malloc(
		param.p_swapchain_details->present_modes_count * sizeof(VkPresentModeKHR)
	);
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		phy_devices[i], param.chosen_surface,
		&param.p_swapchain_details->present_modes_count,
		param.p_swapchain_details->present_modes
	);


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





int create_command_pool(GlobalData* p_win) {
	VkResult result = VK_SUCCESS;
	int res = 0;

	VkCommandPoolCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = p_win->graphics_inx,
	};
	result = vkCreateCommandPool(p_win->device, &create_info,
		p_win->p_host_alloc_calls,
		&(p_win->cmd_pool));
	res--;
	if (result != VK_SUCCESS)
		return res;

	res = 0;

	return res;
}



int clear_command_pool(GlobalData* p_win) {
	vkDestroyCommandPool(p_win->device, p_win->cmd_pool,
		p_win->p_host_alloc_calls);
	return 0;
}


int clear_device(GlobalData* p_win) {
	vkDestroyDevice(p_win->device,
		p_win->p_host_alloc_calls);
	p_win->device = p_win->phy_device = VK_NULL_HANDLE;
	return 0;
}

int clear_instance(VkAllocationCallbacks* alloc_callbacks) {
	vkDestroyInstance(g_vk_instance, alloc_callbacks);
	g_vk_instance = VK_NULL_HANDLE;
	return 0;
}