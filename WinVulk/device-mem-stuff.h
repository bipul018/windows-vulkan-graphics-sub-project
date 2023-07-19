#pragma once

#include "common-stuff.h"

struct GPUAllocator {
    VkDeviceMemory memory_handle;

    // Will be mapped iff host visible
    void *mapped_memory;
    VkDeviceSize mem_size;
    VkDeviceSize curr_offset;
};

typedef struct GPUAllocator GPUAllocator;

enum AllocateDeviceMemoryCodes {
    ALLOCATE_DEVICE_MEMORY_FAILED = -0x7fff,
    ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE,
    ALLOCATE_DEVICE_MEMORY_OK = 0,
};

typedef struct {
    VkPhysicalDevice phy_device;
    VkDevice device;
    // like host visible, local
    uint32_t memory_properties;
    uint32_t memory_type_flag_bits;
    size_t allocation_size;

    GPUAllocator *p_gpu_allocr;
} AllocateDeviceMemoryParam;

int allocate_device_memory(VkAllocationCallbacks *alloc_callbacks,
                           AllocateDeviceMemoryParam param) {

    VkPhysicalDeviceMemoryProperties mem_props = { 0 };

    vkGetPhysicalDeviceMemoryProperties(param.phy_device, &mem_props);

    int32_t mem_inx = -1;
    for (int i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (((1 << i) & param.memory_type_flag_bits) &&
            (param.memory_properties ==
             (param.memory_properties &
              mem_props.memoryTypes[i].propertyFlags))) {
            mem_inx = i;
            break;
        }
    }
    if (mem_inx == -1)
        return ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE;

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = mem_inx,
        .allocationSize = param.allocation_size,
    };

    if (vkAllocateMemory(param.device, &alloc_info, alloc_callbacks,
                         &(param.p_gpu_allocr->memory_handle)) !=
        VK_SUCCESS)
        return ALLOCATE_DEVICE_MEMORY_FAILED;

    param.p_gpu_allocr->mem_size = param.allocation_size;
    param.p_gpu_allocr->curr_offset = 0;

    if (param.memory_properties &
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        if (vkMapMemory(
              param.device, param.p_gpu_allocr->memory_handle,
              param.p_gpu_allocr->curr_offset,
              param.p_gpu_allocr->mem_size, 0,
              &(param.p_gpu_allocr->mapped_memory)) != VK_SUCCESS)

            param.p_gpu_allocr->mapped_memory = NULL;
    } else {
        param.p_gpu_allocr->mapped_memory = NULL;
    }

    return ALLOCATE_DEVICE_MEMORY_OK;
}

typedef struct {
    VkDevice device;

    GPUAllocator *p_gpu_allocr;
} FreeDeviceMemoryParam;

void free_device_memory(VkAllocationCallbacks *alloc_callbacks,
                        FreeDeviceMemoryParam param, int err_codes) {

    switch (err_codes) {
    case ALLOCATE_DEVICE_MEMORY_OK:

        if (param.p_gpu_allocr->mapped_memory) {
            vkUnmapMemory(param.device,
                          param.p_gpu_allocr->memory_handle);
        }
        vkFreeMemory(param.device, param.p_gpu_allocr->memory_handle,
                     alloc_callbacks);

    case ALLOCATE_DEVICE_MEMORY_FAILED:
    case ALLOCATE_DEVICE_MEMORY_NOT_AVAILABLE_TYPE:
        param.p_gpu_allocr->memory_handle = VK_NULL_HANDLE;
        param.p_gpu_allocr->mapped_memory = NULL;
        param.p_gpu_allocr->mem_size = 0;
        param.p_gpu_allocr->curr_offset = 0;
    }
}

enum GpuAllocrAllocateBufferCodes {
    GPU_ALLOCR_ALLOCATE_BUFFER_FAILED = -0x7fff,
    GPU_ALLOCR_ALLOCATE_BUFFER_NOT_ENOUGH_MEMORY,
    GPU_ALLOCR_ALLOCTE_BUFFER_OK = 0
};
typedef struct {
    VkBuffer buffer;
    size_t total_amt;
    size_t base_align;
    void *mapped_memory;
} GpuAllocrAllocatedBuffer;
int gpu_allocr_allocate_buffer(GPUAllocator *gpu_allocr,
                               VkDevice device,
                               GpuAllocrAllocatedBuffer *buffer_info) {

    buffer_info->base_align = max(1, buffer_info->base_align);
    size_t next_offset =
      align_up_(gpu_allocr->curr_offset, buffer_info->base_align) +
      buffer_info->total_amt;
    if (next_offset > gpu_allocr->mem_size)
        return GPU_ALLOCR_ALLOCATE_BUFFER_NOT_ENOUGH_MEMORY;

    if (vkBindBufferMemory(device, buffer_info->buffer,
                           gpu_allocr->memory_handle,
                           next_offset - buffer_info->total_amt) != VK_SUCCESS) {
        return GPU_ALLOCR_ALLOCATE_BUFFER_FAILED;
    }
    if (gpu_allocr->mapped_memory)
        buffer_info->mapped_memory =
          (uint8_t *)(gpu_allocr->mapped_memory) +
          next_offset - buffer_info->total_amt;
    
    gpu_allocr->curr_offset = next_offset;
    return GPU_ALLOCR_ALLOCTE_BUFFER_OK;
}
