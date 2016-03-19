#include "engine/scene.hpp"

#include <memory>
#include <vector>
#include <stdexcept>
#include <GLFW/glfw3.h>

#include "common/error_checking.hpp"

namespace engine {

/******************************************************
*                  GET_INSTANCE_PROC_ADDR             *
*******************************************************/
#define GET_INSTANCE_PROC_ADDR(instance, app, entrypoint) {                                \
  app.entryPoints.fp##entrypoint =                                                         \
    (PFN_vk##entrypoint)instance.getProcAddr("vk" #entrypoint);                            \
  if (app.entryPoints.fp##entrypoint == nullptr) {                                         \
    throw std::runtime_error("vk::getInstanceProcAddr failed to find vk" #entrypoint);     \
  }                                                                                        \
}


/******************************************************
*                      CreateInstance                 *
*******************************************************/
vk::Instance Scene::CreateInstance(VulkanApplication& app) {
  /* Look for instance validation layers */
  uint32_t allInstanceLayerCount = 0;
  vk::chk(vk::enumerateInstanceLayerProperties(&allInstanceLayerCount, nullptr));

  if (allInstanceLayerCount > 0) {
    std::unique_ptr<vk::LayerProperties> instanceLayers{
        new vk::LayerProperties[allInstanceLayerCount]};
    vk::chk(vk::enumerateInstanceLayerProperties(&allInstanceLayerCount,
                                                 instanceLayers.get()));

#if VK_VALIDATE
      CheckForMissingLayers(app.instanceValidationLayers.size(),
                            app.instanceValidationLayers.data(),
                            allInstanceLayerCount,
                            instanceLayers.get());
#endif
  }

  /* Look for instance extensions */
  unsigned required_extension_count;
  const char** required_extensions =
      glfwGetRequiredInstanceExtensions(&required_extension_count);
  if (!required_extensions) {
    throw std::runtime_error("glfwGetRequiredInstanceExtensions failed to find the "
                             "platform surface extensions.\n\nDo you have a compatible "
                             "Vulkan installable client driver (ICD) installed?");
  }

  for (uint32_t i = 0; i < required_extension_count; i++) {
    app.instanceExtensionNames.push_back(required_extensions[i]);
  }

  vk::InstanceCreateInfo inst_info = vk::InstanceCreateInfo()
      .pApplicationInfo(&app.applicationInfo)
      .enabledLayerCount(app.instanceValidationLayers.size())
      .ppEnabledLayerNames((const char* const*)app.instanceValidationLayers.data())
      .enabledExtensionCount(app.instanceExtensionNames.size())
      .ppEnabledExtensionNames((const char* const*)app.instanceExtensionNames.data());

  vk::Instance instance;
  vk::Result err = vk::createInstance(&inst_info, nullptr, &instance);
  if (err == vk::Result::eErrorIncompatibleDriver) {
      throw std::runtime_error("Cannot find a compatible Vulkan installable "
                               "client driver (ICD).");
  } else if (err == vk::Result::eErrorExtensionNotPresent) {
      throw std::runtime_error("Cannot find a specified extension library"
                               ".\nMake sure your layers path is set appropriately");
  } else if (err != vk::Result::eSuccess) {
      throw std::runtime_error("vk::createInstance failed.\n\nDo you have a "
                               "compatible Vulkan installable client driver "
                               "(ICD) installed?");
  }

  // Having these GIPA queries of device extension entry points both
  // BEFORE and AFTER vk::createDevice is a good test for the loader
  GET_INSTANCE_PROC_ADDR(instance, app, GetPhysicalDeviceSurfaceCapabilitiesKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, GetPhysicalDeviceSurfaceFormatsKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, GetPhysicalDeviceSurfacePresentModesKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, GetPhysicalDeviceSurfaceSupportKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, CreateSwapchainKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, DestroySwapchainKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, GetSwapchainImagesKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, AcquireNextImageKHR);
  GET_INSTANCE_PROC_ADDR(instance, app, QueuePresentKHR);

  return instance;
}


/******************************************************
*                   CreatePhysicalDevice              *
*******************************************************/
vk::PhysicalDevice Scene::CreatePhysicalDevice(vk::Instance& instance,
                                              const VulkanApplication& app) {
  /* Make initial call to query gpu_count, then second call for gpu info*/
  uint32_t gpu_count;
  vk::chk(instance.enumeratePhysicalDevices(&gpu_count, nullptr));

  vk::PhysicalDevice gpuToUse;
  if (gpu_count > 0) {
      std::unique_ptr<vk::PhysicalDevice> physical_devices{
          new vk::PhysicalDevice[gpu_count]};
      vk::chk(instance.enumeratePhysicalDevices(&gpu_count, physical_devices.get()));

      /* TODO */
      gpuToUse = physical_devices.get()[0];
  } else {
      throw std::runtime_error("vk::enumeratePhysicalDevices reported zero accessible devices."
                               "\n\nDo you have a compatible Vulkan installable client"
                               " driver (ICD) installed?");
  }

  /* Look for validation layers */
  uint32_t device_layer_count = 0;
  vk::chk(gpuToUse.enumerateDeviceLayerProperties(&device_layer_count, nullptr));

  if (device_layer_count > 0) {
    std::unique_ptr<vk::LayerProperties> device_layers{
        new vk::LayerProperties[device_layer_count]};
    vk::chk(gpuToUse.enumerateDeviceLayerProperties(&device_layer_count,
                                                    device_layers.get()));

#if VK_VALIDATE
      CheckForMissingLayers(app.deviceValidationLayers.size(),
                            app.deviceValidationLayers.data(),
                            device_layer_count,
                            device_layers.get());
#endif
  }

  return gpuToUse;
}



/******************************************************
*                   CreateSurface                     *
*******************************************************/
VkSurfaceKHR Scene::CreateSurface(const vk::Instance& instance,
                                  GLFWwindow* window) {

  VkSurfaceKHR surface;

  // Create a WSI surface for the window:
  glfwCreateWindowSurface(instance, window, nullptr, &surface);

  return surface;
}


/******************************************************
*             SelectQraphicsQueueNodeIndex            *
*******************************************************/
uint32_t Scene::SelectQraphicsQueueNodeIndex(const vk::PhysicalDevice& gpu,
                                             const VkSurfaceKHR& surface,
                                             const VulkanApplication& app) {
  // Get queue count and properties
  uint32_t queueCount = 0;
  gpu.getQueueFamilyProperties(&queueCount, nullptr);

  std::vector<vk::QueueFamilyProperties> queue_props;
  queue_props.resize(queueCount);
  gpu.getQueueFamilyProperties(&queueCount, queue_props.data());
  assert(1 <= queueCount);
  queue_props.resize(queueCount);

  // Graphics queue and MemMgr queue can be separate.
  // TODO: Add support for separate queues, including synchronization,
  //       and appropriate tracking for QueueSubmit

  // Iterate over each queue to learn whether it supports presenting:
  std::unique_ptr<vk::Bool32> supportsPresent{new vk::Bool32[queueCount]};
  for (uint32_t i = 0; i < queueCount; i++) {
    app.entryPoints.fpGetPhysicalDeviceSurfaceSupportKHR(
        gpu, i, surface, &supportsPresent.get()[i]);
  }

  // Search for a graphics and a present queue in the array of queue
  // families, try to find one that supports both
  uint32_t graphicsQueueNodeIndex = UINT32_MAX;
  uint32_t presentQueueNodeIndex = UINT32_MAX;
  for (uint32_t i = 0; i < queueCount; i++) {
    if ((queue_props[i].queueFlags() & vk::QueueFlagBits::eGraphics) != 0) {
      if (graphicsQueueNodeIndex == UINT32_MAX) {
        graphicsQueueNodeIndex = i;
      }

      if (supportsPresent.get()[i]) {
        graphicsQueueNodeIndex = i;
        presentQueueNodeIndex = i;
        break;
      }
    }
  }

  if (presentQueueNodeIndex == UINT32_MAX) {
    // If didn't find a queue that supports both graphics and present, then
    // find a separate present queue.
    for (uint32_t i = 0; i < queueCount; ++i) {
      if (supportsPresent.get()[i]) {
        presentQueueNodeIndex = i;
        break;
      }
    }
  }

  // Generate error if could not find both a graphics and a present queue
  if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) {
    throw std::runtime_error("Could not find a graphics and a present queue");
  }

  // TODO: Add support for separate queues, including presentation,
  //       synchronization, and appropriate tracking for QueueSubmit.
  // NOTE: While it is possible for an application to use a separate graphics
  //       and a present queues, this demo program assumes it is only using
  //       one:
  if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
    throw std::runtime_error("Could not find a common graphics and a present queue");
  }

  return graphicsQueueNodeIndex;
}


/******************************************************
*                   GET_DEVICE_PROC_ADDR              *
*******************************************************/
#define GET_DEVICE_PROC_ADDR(device, app, entrypoint) {                                  \
  app.entryPoints.fp##entrypoint =                                                       \
    (PFN_vk##entrypoint)device.getProcAddr("vk" #entrypoint);                            \
  if (app.entryPoints.fp##entrypoint == nullptr) {                                       \
    throw std::runtime_error("vk::getDeviceProcAddr failed to find vk" #entrypoint);     \
  }                                                                                      \
}

/******************************************************
*                   CreateDevice                      *
*******************************************************/
vk::Device Scene::CreateDevice(const vk::PhysicalDevice& gpu,
                               uint32_t graphicsQueueNodeIndex,
                               VulkanApplication& app) {
  float queue_priorities[1] = {0.0};
  const vk::DeviceQueueCreateInfo queue = vk::DeviceQueueCreateInfo()
      .queueFamilyIndex(graphicsQueueNodeIndex)
      .queueCount(1)
      .pQueuePriorities(queue_priorities);

  vk::PhysicalDeviceFeatures features = vk::PhysicalDeviceFeatures()
      .fillModeNonSolid(true);

  vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
      .queueCreateInfoCount(1)
      .pQueueCreateInfos(&queue)
      .enabledLayerCount(app.deviceValidationLayers.size())
      .ppEnabledLayerNames(app.deviceValidationLayers.data())
      .enabledExtensionCount(app.deviceExtensionNames.size())
      .ppEnabledExtensionNames(app.deviceExtensionNames.data())
      .pEnabledFeatures(&features);

  vk::Device device;
  vk::chk(gpu.createDevice(&deviceCreateInfo, nullptr, &device));

  GET_DEVICE_PROC_ADDR(device, app, CreateSwapchainKHR);
  GET_DEVICE_PROC_ADDR(device, app, DestroySwapchainKHR);
  GET_DEVICE_PROC_ADDR(device, app, GetSwapchainImagesKHR);
  GET_DEVICE_PROC_ADDR(device, app, AcquireNextImageKHR);
  GET_DEVICE_PROC_ADDR(device, app, QueuePresentKHR);

  return device;
}


/******************************************************
*                       GetQueue                      *
*******************************************************/
vk::Queue Scene::GetQueue(const vk::Device& device,
                          uint32_t graphicsQueueNodeIndex) {
  vk::Queue queue;
  device.getQueue(graphicsQueueNodeIndex, 0, &queue);
  return queue;
}


/******************************************************
*                  GetSurfaceProperties               *
*******************************************************/
void Scene::GetSurfaceProperties(const vk::PhysicalDevice& gpu,
                                 const VkSurfaceKHR& surface,
                                 const VulkanApplication& app,
                                 vk::Format& format,
                                 vk::ColorSpaceKHR& colorSpace,
                                 vk::PhysicalDeviceMemoryProperties& memoryProperties) {

    // Get the list of vk::Format's that are supported:
    uint32_t formatCount;
    VkChk(app.entryPoints.fpGetPhysicalDeviceSurfaceFormatsKHR(
          gpu, surface, &formatCount, nullptr));

    std::unique_ptr<VkSurfaceFormatKHR> surfFormats{new VkSurfaceFormatKHR[formatCount]};
    VkChk(app.entryPoints.fpGetPhysicalDeviceSurfaceFormatsKHR(
        gpu, surface, &formatCount, surfFormats.get()));

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats.get()[0].format == VK_FORMAT_UNDEFINED) {
        format = vk::Format::eB8G8R8A8Unorm;
    } else {
        assert(formatCount >= 1);
        format = static_cast<vk::Format>(surfFormats.get()[0].format);
    }
    colorSpace = static_cast<vk::ColorSpaceKHR>(surfFormats.get()[0].colorSpace);

    // Get Memory information and properties
    gpu.getMemoryProperties(&memoryProperties);
}

#if VK_VALIDATE

/******************************************************
*                  CheckForMissingLayers              *
*******************************************************/
void Scene::CheckForMissingLayers(uint32_t check_count,
                                  const char* const* check_names,
                                  uint32_t layer_count,
                                  vk::LayerProperties* layers) {
  bool found_all = true;
  for (uint32_t i = 0; i < check_count; i++) {
    bool found = false;
    for (uint32_t j = 0; !found && j < layer_count; j++) {
      found = !std::strcmp(check_names[i], layers[j].layerName());
    }
    if (!found) {
      std::cerr << "Cannot find layer: " << check_names[i] << std::endl;
      found_all = false;
    }
  }

  if (!found_all) {
    throw std::runtime_error("Couldn't find all requested validation layers.");
  }
}

#endif // VK_VALIDATE

}
