#pragma once

#include "common-stuff.h"

struct GPUAllocator {
    VkDeviceMemory memory_handle;

    // Will be mapped iff host visible
    void *mapped_memory;
    VkDeviceSize mem_size;
    VkDeviceSize curr_offset;
    size_t atom_size;
    uint32_t memory_type;
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

int allocate_device_memory(const VkAllocationCallbacks *alloc_callbacks,
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

    VkPhysicalDeviceProperties device_props;
    vkGetPhysicalDeviceProperties(param.phy_device, &device_props);

    param.p_gpu_allocr->atom_size =
      device_props.limits.nonCoherentAtomSize;
    param.p_gpu_allocr->memory_type = mem_inx;


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

void free_device_memory(const VkAllocationCallbacks *alloc_callbacks,
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
int gpu_allocr_allocate_buffer(
  GPUAllocator *gpu_allocr, VkDevice device,
  GpuAllocrAllocatedBuffer *buffer_info) {

    buffer_info->base_align = max(1, buffer_info->base_align);
    size_t next_offset =
      align_up_(gpu_allocr->curr_offset, buffer_info->base_align) +
      buffer_info->total_amt;
    if (next_offset > gpu_allocr->mem_size)
        return GPU_ALLOCR_ALLOCATE_BUFFER_NOT_ENOUGH_MEMORY;

    if (vkBindBufferMemory(
          device, buffer_info->buffer, gpu_allocr->memory_handle,
          next_offset - buffer_info->total_amt) != VK_SUCCESS) {
        return GPU_ALLOCR_ALLOCATE_BUFFER_FAILED;
    }
    if (gpu_allocr->mapped_memory)
        buffer_info->mapped_memory =
          (uint8_t *)(gpu_allocr->mapped_memory) + next_offset -
          buffer_info->total_amt;

    gpu_allocr->curr_offset = next_offset;
    return GPU_ALLOCR_ALLOCTE_BUFFER_OK;
}

// Only for host visible and properly mapped memories
VkResult gpu_allocr_flush_memory(VkDevice device, GPUAllocator allocr,
                                 void *mapped_memory, size_t amount) {

    VkDeviceSize offset =
      (uint8_t *)mapped_memory - (uint8_t *)allocr.mapped_memory;

    VkDeviceSize final = offset + amount;
    offset = align_down_(offset, allocr.atom_size);
    final = offset + align_up_(final - offset, allocr.atom_size);
    final = min(final, allocr.mem_size);


    return vkFlushMappedMemoryRanges(
      device, 1,
      &(VkMappedMemoryRange){
        .memory = allocr.memory_handle,
        .offset = offset,
        .size = final - offset,
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE });
}

//Assumes a compatible memory is allocated
enum CreateAndAllocateBufferCodes {
    CREATE_AND_ALLOCATE_BUFFER_DUMMY_ERR_CODE = -0x7fff,
    CREATE_AND_ALLOCATE_BUFFER_BUFFER_ALLOC_FAILED,
    CREATE_AND_ALLOCATE_BUFFER_INCOMPATIBLE_MEMORY,
    CREATE_AND_ALLOCATE_BUFFER_CREATE_BUFFER_FAILED,


    CREATE_AND_ALLOCATE_BUFFER_OK = 0,
};


typedef struct {
    GpuAllocrAllocatedBuffer *p_buffer;
    VkSharingMode share_mode;
    VkBufferUsageFlags usage;
    size_t size;

}CreateAndAllocateBufferParam;
int create_and_allocate_buffer(const VkAllocationCallbacks * alloc_callbacks,
                               GPUAllocator *p_allocr,
                               VkDevice device,
                               CreateAndAllocateBufferParam param) { 
    
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = param.usage,
        .sharingMode = param.share_mode,
        .size = param.size,
    };

    if (vkCreateBuffer(device, &create_info, alloc_callbacks,
                       &param.p_buffer->buffer) != VK_SUCCESS)
        return CREATE_AND_ALLOCATE_BUFFER_CREATE_BUFFER_FAILED;

    //VkDeviceBufferMemoryRequirements info_struct = {
    //    .sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS,
    //    .pCreateInfo = &create_info,
    //};
    //VkMemoryRequirements2 mem_reqs = {
    //    .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    //};
    //vkGetDeviceBufferMemoryRequirements(device, &info_struct,
    //                                    &mem_reqs);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, param.p_buffer->buffer,
                                  &mem_reqs);

    param.p_buffer->base_align = mem_reqs.alignment;
    param.p_buffer->total_amt = mem_reqs.size;
    

    //if ((mem_reqs.memoryRequirements.memoryTypeBits &
    if ((mem_reqs.memoryTypeBits &
         p_allocr->memory_type) == 0)
        return CREATE_AND_ALLOCATE_BUFFER_INCOMPATIBLE_MEMORY;

    if (gpu_allocr_allocate_buffer(p_allocr, device, param.p_buffer) <
        0)
        return CREATE_AND_ALLOCATE_BUFFER_BUFFER_ALLOC_FAILED;

    return CREATE_AND_ALLOCATE_BUFFER_OK;
}