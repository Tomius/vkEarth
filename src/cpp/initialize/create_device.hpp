#ifndef CREATE_DEVICE_HPP_
#define CREATE_DEVICE_HPP_

#include <vulkan/vk_cpp.h>

#include "common/vulkan_application.hpp"

namespace Initialize {

vk::Device CreateDevice(const vk::PhysicalDevice& gpu,
                        uint32_t graphicsQueueNodeIndex,
                        VulkanApplication& app);

}

#endif // CREATE_DEVICE_HPP_
