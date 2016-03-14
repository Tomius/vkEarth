#include "initialize/create_device.hpp"

#include <memory>
#include <iostream>
#include <GLFW/glfw3.h>

#include "common/defines.hpp"
#include "common/error_checking.hpp"

#define GET_DEVICE_PROC_ADDR(device, app, entrypoint) {                                    \
    app.entryPoints.fp##entrypoint =                                                       \
      (PFN_vk##entrypoint)device.getProcAddr("vk" #entrypoint);                            \
    if (app.entryPoints.fp##entrypoint == nullptr) {                                       \
      throw std::runtime_error("vk::getDeviceProcAddr failed to find vk" #entrypoint);     \
    }                                                                                      \
  }

vk::Device Initialize::CreateDevice(const vk::PhysicalDevice& gpu,
                                    uint32_t graphicsQueueNodeIndex,
                                    VulkanApplication& app) {
  float queue_priorities[1] = {0.0};
  const vk::DeviceQueueCreateInfo queue = vk::DeviceQueueCreateInfo()
      .queueFamilyIndex(graphicsQueueNodeIndex)
      .queueCount(1)
      .pQueuePriorities(queue_priorities);

  vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
      .queueCreateInfoCount(1)
      .pQueueCreateInfos(&queue)
      .enabledLayerCount(app.deviceValidationLayers.size())
      .ppEnabledLayerNames(app.deviceValidationLayers.data())
      .enabledExtensionCount(app.deviceExtensionNames.size())
      .ppEnabledExtensionNames(app.deviceExtensionNames.data());

  vk::Device device;
  vk::chk(gpu.createDevice(&deviceCreateInfo, nullptr, &device));

  GET_DEVICE_PROC_ADDR(device, app, CreateSwapchainKHR);
  GET_DEVICE_PROC_ADDR(device, app, DestroySwapchainKHR);
  GET_DEVICE_PROC_ADDR(device, app, GetSwapchainImagesKHR);
  GET_DEVICE_PROC_ADDR(device, app, AcquireNextImageKHR);
  GET_DEVICE_PROC_ADDR(device, app, QueuePresentKHR);

  return device;
}
