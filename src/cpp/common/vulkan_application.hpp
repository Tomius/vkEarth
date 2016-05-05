// Copyright (c) 2016, Tamas Csala

#ifndef VULKAN_COMMON_HPP_
#define VULKAN_COMMON_HPP_

#include <string>
#include <vector>
#include <vulkan/vk_cpp.hpp>

#include "common/settings.hpp"

struct VulkanApplication {
  const vk::ApplicationInfo application_info = vk::ApplicationInfo()
      .pApplicationName("vkEarth")
      .applicationVersion(0)
      .pEngineName("vkEarth")
      .engineVersion(0)
      .apiVersion(VK_MAKE_VERSION(1, 0, 11));

  std::vector<const char*> instance_validation_layers{
#if VK_VALIDATE
    //"VK_LAYER_LUNARG_param_checker",
    //"VK_LAYER_LUNARG_device_limits",
    //"VK_LAYER_LUNARG_object_tracker",
    /*"VK_LAYER_LUNARG_image",
    "VK_LAYER_LUNARG_mem_tracker",
    "VK_LAYER_LUNARG_draw_state",
    "VK_LAYER_LUNARG_swapchain",*/
    //"VK_LAYER_GOOGLE_unique_objects"
#endif
  };

  std::vector<const char*> instance_extension_names{
#if VK_VALIDATE
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
  };

  std::vector<const char*> device_validation_layers{
/*#if VK_VALIDATE
    "VK_LAYER_LUNARG_param_checker",
    "VK_LAYER_LUNARG_device_limits",
    "VK_LAYER_LUNARG_object_tracker",
    "VK_LAYER_LUNARG_image",
    "VK_LAYER_LUNARG_mem_tracker",
    "VK_LAYER_LUNARG_draw_state",
    "VK_LAYER_LUNARG_swapchain",
    "VK_LAYER_GOOGLE_unique_objects"
#endif*/
  };

  std::vector<const char*> device_extension_names{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  struct GlfwVulkanEntryPoints {
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR
        GetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        GetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        GetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        GetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR QueuePresentKHR = nullptr;
  } entry_points;
};


#endif
