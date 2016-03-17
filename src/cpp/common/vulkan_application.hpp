#ifndef VULKAN_COMMON_H_
#define VULKAN_COMMON_H_

#include <string>
#include <vector>
#include <vulkan/vk_cpp.h>

#include "common/defines.hpp"

struct VulkanApplication {
  const vk::ApplicationInfo applicationInfo = vk::ApplicationInfo()
      .pApplicationName("vkEarth")
      .applicationVersion(0)
      .pEngineName("vkEarth")
      .engineVersion(0)
      .apiVersion(VK_MAKE_VERSION(1, 0, 4));

  std::vector<const char*> instanceValidationLayers{
#if VK_VALIDATE
    // "VK_LAYER_GOOGLE_threading",
    "VK_LAYER_LUNARG_param_checker",
    "VK_LAYER_LUNARG_device_limits",
    "VK_LAYER_LUNARG_object_tracker",
    "VK_LAYER_LUNARG_image",
    "VK_LAYER_LUNARG_mem_tracker",
    "VK_LAYER_LUNARG_draw_state",
    "VK_LAYER_LUNARG_swapchain",
    "VK_LAYER_GOOGLE_unique_objects"
#endif
  };

  std::vector<const char*> instanceExtensionNames{
#if VK_VALIDATE
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
  };

  std::vector<const char*> deviceValidationLayers{
#if VK_VALIDATE
    // "VK_LAYER_GOOGLE_threading",
    "VK_LAYER_LUNARG_param_checker",
    "VK_LAYER_LUNARG_device_limits",
    "VK_LAYER_LUNARG_object_tracker",
    "VK_LAYER_LUNARG_image",
    "VK_LAYER_LUNARG_mem_tracker",
    "VK_LAYER_LUNARG_draw_state",
    "VK_LAYER_LUNARG_swapchain",
    "VK_LAYER_GOOGLE_unique_objects"
#endif
  };

  std::vector<const char*> deviceExtensionNames{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  struct GlfwVulkanEntryPoints {
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR
        fpGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        fpGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        fpGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        fpGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR fpQueuePresentKHR = nullptr;
  } entryPoints;
};


#endif
