#ifndef CREATE_PHYSICAL_DEVICE_HPP_
#define CREATE_PHYSICAL_DEVICE_HPP_

#include "common/vulkan_application.hpp"

namespace Initialize {

vk::PhysicalDevice CreatePhysicalDevice(vk::Instance& instance,
                                        const VulkanApplication& app);

}


#endif //CREATE_PHYSICAL_DEVICE_HPP_
