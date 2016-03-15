#include "initialize/prepare_swapchain.hpp"

#include <memory>
#include <vector>

#include "common/error_checking.hpp"

VkSurfaceKHR Initialize::CreateSurface(const vk::Instance& instance,
                                       GLFWwindow* window) {

  VkSurfaceKHR surface;

  // Create a WSI surface for the window:
  glfwCreateWindowSurface(instance, window, nullptr, &surface);

  return surface;
}

uint32_t Initialize::SelectQraphicsQueueNodeIndex(const vk::PhysicalDevice& gpu,
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


vk::Queue Initialize::GetQueue(const vk::Device& device,
                               uint32_t graphicsQueueNodeIndex) {
  vk::Queue queue;
  device.getQueue(graphicsQueueNodeIndex, 0, &queue);
  return queue;
}


void Initialize::GetSurfaceProperties(const vk::PhysicalDevice& gpu,
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
