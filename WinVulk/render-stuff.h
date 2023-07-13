#pragma once


//Data like render passes, vertex buffers, descriptors, synchronization objects 
struct RenderData {

	VkRenderPass render_pass;

	UINT curr_frame_in_flight;
	UINT max_frames_in_flight;
	VkCommandBuffer* cmd_buffer;
	VkFence* in_flight_fences;
	VkSemaphore* image_available_semaphores;
	VkSemaphore* render_finished_semaphores;


	VkBuffer vert_buff;
	VkDescriptorPool descriptor_pool;


	//Temporary members
	VkBuffer* uniform_buffer;
	VkDescriptorSet* descriptor_sets;
};
typedef struct RenderData RenderData;


//Pipeline data 
struct GraphicsPipeline {

	VkPipelineLayout graphics_pipeline_layout;
	VkPipeline graphics_pipeline;
	VkDescriptorSetLayout descriptor_layout;

};
typedef struct GraphicsPipeline GraphicsPipeline;


int create_render_pass(GlobalData* p_win) {
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
		.pDependencies = &subpass_dependency,
	};

	result = vkCreateRenderPass(p_win->device, &create_info,
		p_win->p_host_alloc_calls,
		&(p_win->render_pass));

	res--;
	if (result != VK_SUCCESS)
		goto create_render_pass;

	res = 0;

create_render_pass:
	return res;
}


uint32_t* read_bytecode_file(const char* file_name, size_t* file_size) {
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

	//Assume that p_win is aligned properly
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

int create_shader_module(VkDevice device, uint32_t* shader_bytecode,
	size_t code_size, VkShaderModule* p_shader_module,
	VkAllocationCallbacks* alloc_callbacks) {

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

	result = vkCreateShaderModule(device, &create_info,
		alloc_callbacks,
		p_shader_module);

	res--;
	if (result != VK_SUCCESS)
		return res;

	return 0;
}


int create_graphics_pipeline(GlobalData* p_win) {
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
	if (create_shader_module(p_win->device, vert_shader_code, vert_shader_size,
		&vert_shader,
		p_win->p_host_alloc_calls) < 0) {
		free(vert_shader_code);
		free(frag_shader_code);
		return res;
	}
	free(vert_shader_code);

	res--;
	if (create_shader_module(p_win->device, frag_shader_code, frag_shader_size,
		&frag_shader,
		p_win->p_host_alloc_calls) < 0) {
		vkDestroyShaderModule(p_win->device, vert_shader,
			p_win->p_host_alloc_calls);
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
		.minDepth = 0.f,
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

	result = vkCreatePipelineLayout(p_win->device, &layout_info,
		p_win->p_host_alloc_calls,
		&(p_win->graphics_pipeline_layout));
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
		1, &create_info,
		p_win->p_host_alloc_calls,
		&(p_win->graphics_pipeline));

	res--;
	if (result != VK_SUCCESS)
		goto create_graphics_pipeline;

	res = 0;

create_graphics_pipeline:

layout_info:
	vkDestroyShaderModule(p_win->device, vert_shader,
		p_win->p_host_alloc_calls);
	vkDestroyShaderModule(p_win->device, frag_shader,
		p_win->p_host_alloc_calls);

	return res;
}


int create_sync_objects(GlobalData* p_win) {
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

		result = vkCreateFence(p_win->device, &fence_create_info,
			p_win->p_host_alloc_calls,
			p_win->in_flight_fences + i);
		res--;
		if (result != VK_SUCCESS)
			return res;

		result = vkCreateSemaphore(p_win->device, &sema_create_info,
			p_win->p_host_alloc_calls,
			p_win->image_available_semaphores + i);
		res--;
		if (result != VK_SUCCESS)
			return res;

		result = vkCreateSemaphore(p_win->device, &sema_create_info,
			p_win->p_host_alloc_calls,
			p_win->render_finished_semaphores + i);
		res--;
		if (result != VK_SUCCESS)
			return res;
	}
	res = 0;

clear:
	return res;
}


int create_command_buffers(GlobalData* p_win) {

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


int create_descriptor_layout(GlobalData* p_win) {

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


	result = vkCreateDescriptorSetLayout(p_win->device, &create_info,
		p_win->p_host_alloc_calls,
		&p_win->descriptor_layout);
	res--;
	if (result != VK_SUCCESS)
		return res;


	res = 0;


	return res;

}





int clear_descriptor_layout(GlobalData* p_win) {
	vkDestroyDescriptorSetLayout(p_win->device, p_win->descriptor_layout,
		p_win->p_host_alloc_calls);
	return 0;
}


int clear_sync_objects(GlobalData* p_win) {

	for (int i = 0; i < p_win->max_frames_in_flight; ++i) {
		vkDestroySemaphore(p_win->device, p_win->render_finished_semaphores[i],
			p_win->p_host_alloc_calls);
		vkDestroySemaphore(p_win->device, p_win->image_available_semaphores[i],
			p_win->p_host_alloc_calls);
		vkDestroyFence(p_win->device, p_win->in_flight_fences[i],
			p_win->p_host_alloc_calls);
	}
	free(p_win->in_flight_fences);
	free(p_win->image_available_semaphores);
	free(p_win->render_finished_semaphores);

	return 0;
}


int clear_command_buffers(GlobalData* p_win) {
	free(p_win->cmd_buffer);
	return 0;
}


int clear_pipeline(GlobalData* p_win) {
	vkDestroyPipeline(p_win->device, p_win->graphics_pipeline,
		p_win->p_host_alloc_calls);
	vkDestroyPipelineLayout(p_win->device, p_win->graphics_pipeline_layout,
		p_win->p_host_alloc_calls);

	return 0;
}


int clear_render_pass(GlobalData* p_win) {
	vkDestroyRenderPass(p_win->device, p_win->render_pass,
		p_win->p_host_alloc_calls);
	return 0;
}
