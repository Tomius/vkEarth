#ifndef VULKAN_MEMORY_HPP_
#define VULKAN_MEMORY_HPP_

#include <vulkan/vk_cpp.h>

inline void MemoryTypeFromProperties(const vk::PhysicalDeviceMemoryProperties& memoryProperties,
                                     uint32_t typeBits,
                                     vk::MemoryPropertyFlags requirements_mask,
                                     vk::MemoryAllocateInfo& mem_alloc) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memoryProperties.memoryTypes()[i].propertyFlags() & requirements_mask)
                == requirements_mask) {
              mem_alloc.memoryTypeIndex(i);
              return;
            }
        }
        typeBits >>= 1;
    }

    throw std::runtime_error("Couldn't find request memory type.");
}

#endif // VULKAN_MEMORY_HPP_
