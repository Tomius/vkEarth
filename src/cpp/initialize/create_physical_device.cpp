#include "initialize/create_physical_device.hpp"

#include <memory>
#include <GLFW/glfw3.h>
#include "common/error_checking.hpp"
#include "initialize/validation_layer_checker.hpp"

#define GET_INSTANCE_PROC_ADDR(instance, app, entrypoint) {                                  \
    app.entryPoints.fp##entrypoint =                                                         \
      (PFN_vk##entrypoint)instance.getProcAddr("vk" #entrypoint);                            \
    if (app.entryPoints.fp##entrypoint == nullptr) {                                         \
      throw std::runtime_error("vk::getInstanceProcAddr failed to find vk" #entrypoint);     \
    }                                                                                        \
  }

vk::Instance Initialize::CreateInstance(VulkanApplication& app) {
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


vk::PhysicalDevice Initialize::CreatePhysicalDevice(vk::Instance& instance,
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
