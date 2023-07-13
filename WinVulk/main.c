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

#include "common-stuff.h"




int record_command_buffers(GlobalData* p_win, uint32_t img_inx, uint32_t frame_inx) {
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


int draw_frame(GlobalData* p_win) {
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

	GlobalData* p_win = NULL;

	if (msg == WM_CREATE) {
		CREATESTRUCT* cr_data = (CREATESTRUCT*)lparam;
		p_win = (GlobalData*)cr_data->lpCreateParams;
		SetWindowLongPtr(h_wnd, GWLP_USERDATA, (LONG_PTR)p_win);
		
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
		if (vkCreateWin32SurfaceKHR(g_vk_instance, &surface_create_info, p_win->p_host_alloc_calls,
			&(p_win->surface)) != VK_SUCCESS)
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

		result = vkCreateBuffer(p_win->device, &info,
			p_win->p_host_alloc_calls,
			&p_win->vert_buff);

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

			result = vkCreateBuffer(p_win->device, &info,
				p_win->p_host_alloc_calls,
				&p_win->uniform_buffer[i]);

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

		result = vkAllocateMemory(p_win->device, &alloc_info,
			p_win->p_host_alloc_calls,
			&p_win->vert_memory);

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

		result = vkCreateDescriptorPool(p_win->device, &descriptor_pool_info,
			p_win->p_host_alloc_calls,
			&p_win->descriptor_pool);

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
			p_win->max_frames_in_flight );

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
		p_win = (GlobalData*)GetWindowLongPtr(h_wnd, GWLP_USERDATA);
	}

	if (p_win) {

		if (msg == WM_DESTROY) {
			KillTimer(h_wnd, p_win->ex_data.timer_id);
			
			vkDeviceWaitIdle(p_win->device);


			vkDestroyDescriptorPool(p_win->device, p_win->descriptor_pool,
				p_win->p_host_alloc_calls);
			clear_descriptor_layout(p_win);

			vkDestroyBuffer(p_win->device, p_win->vert_buff,
				p_win->p_host_alloc_calls);
			vkFreeMemory(p_win->device, p_win->vert_memory,
				p_win->p_host_alloc_calls);

			for (int i = 0; i < p_win->max_frames_in_flight; ++i) {

				vkDestroyBuffer(p_win->device, p_win->uniform_buffer[i],
					p_win->p_host_alloc_calls);
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
			vkDestroySurfaceKHR(g_vk_instance, p_win->surface,
				p_win->p_host_alloc_calls);
			clear_device(p_win);
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

void* VKAPI_PTR cb_allocation_fxn(
	void* user_data, size_t size, size_t align, VkSystemAllocationScope scope) {

	size_t* ptr = user_data;
	(*ptr) += size;
	printf("%zu : %zu Alloc\t", *ptr,align);
	return _aligned_offset_malloc(size, 8, 0);
}

void* VKAPI_PTR cb_reallocation_fxn(
	void* user_data, void* p_original, size_t size, size_t align, VkSystemAllocationScope scope) {
	size_t* ptr = user_data;
	printf("%zu : %zu Realloc\t", *ptr,align);
	return _aligned_offset_realloc(p_original, size, 8, 0);
}
#include <malloc.h>

void VKAPI_PTR cb_free_fxn(
	void* user_data, void* p_mem) {
	size_t* ptr = user_data;
	//(*ptr) -= _aligned_msize(p_mem, 8, 0);
	printf("%zu Free\t", *ptr);
	return _aligned_free(p_mem);
}

int main(int argc, char* argv[]) {

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
	VkAllocationCallbacks* ptr_alloc_callbacks = &alloc_callbacks;

	ptr_alloc_callbacks = NULL;

	GlobalData wnd_data = { 0 };


	wnd_data.p_host_alloc_calls = ptr_alloc_callbacks;

	if (create_instance(ptr_alloc_callbacks) < 0)
		return -1;

	if (!CreateWindowEx(0, wndclass_name, L"Window", WS_OVERLAPPEDWINDOW , 20, 10, 600, 600, NULL, NULL, h_instance, &wnd_data))
		return -2;

	ShowWindow(wnd_data.win_handle, SW_SHOW);

	//From here on, assumes none tasks fail


	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	clear_instance(ptr_alloc_callbacks);

	UnregisterClass(wnd_class.lpszClassName, h_instance);

	return 0;
}