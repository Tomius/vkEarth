#include "initialize/create_physical_device.hpp"

#include <memory>
#include "common/error_checking.hpp"
#include "initialize/validation_layer_checker.hpp"

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
