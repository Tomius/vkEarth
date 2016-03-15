#ifndef PREPARE_SWAPCHAIN_HPP_
#define PREPARE_SWAPCHAIN_HPP_

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "common/vulkan_application.hpp"

namespace Initialize {

VkSurfaceKHR CreateSurface(const vk::Instance& instance,
                           GLFWwindow* window);

uint32_t SelectQraphicsQueueNodeIndex(const vk::PhysicalDevice& gpu,
                                      const VkSurfaceKHR& surface,
                                      const VulkanApplication& app);

vk::Device CreateDevice(const vk::PhysicalDevice& gpu,
                        uint32_t graphicsQueueNodeIndex,
                        VulkanApplication& app);

vk::Queue GetQueue(const vk::Device& device,
                   uint32_t graphicsQueueNodeIndex);

void GetSurfaceProperties(const vk::PhysicalDevice& gpu,
                          const VkSurfaceKHR& surface,
                          const VulkanApplication& app,
                          vk::Format& format,
                          vk::ColorSpaceKHR& colorSpace,
                          vk::PhysicalDeviceMemoryProperties& memoryProperties);

}

#endif //PREPARE_SWAPCHAIN_HPP_
