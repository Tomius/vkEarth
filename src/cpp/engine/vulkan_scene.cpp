// Copyright (c) 2016, Tamas Csala

#include "engine/vulkan_scene.hpp"

#include <memory>
#include <vector>
#include <stdexcept>
#include <GLFW/glfw3.h>

#include "common/error_checking.hpp"
#include "common/vulkan_memory.hpp"

namespace engine {

/******************************************************
*                          Ctor                       *
*******************************************************/
VulkanScene::VulkanScene(GLFWwindow *window)
    : engine::Scene(window)
    , vk_instance_(CreateInstance(vk_app_))
#if VK_VALIDATE
    , vk_debug_callback_(new DebugCallback(vk_instance_))
#endif
    , vk_gpu_(CreatePhysicalDevice(vk_instance_, vk_app_))
    , vk_surface_(CreateSurface(vk_instance_, window))
    , vk_graphics_queue_node_index_(
        SelectQraphicsQueueNodeIndex(vk_gpu_, vk_surface_, vk_app_))
    , vk_device_(CreateDevice(vk_gpu_, vk_graphics_queue_node_index_, vk_app_))
    , vk_queue_(GetQueue(vk_device_, vk_graphics_queue_node_index_)) {
  GetSurfaceProperties(vk_gpu_, vk_surface_, vk_app_, vk_surface_format_,
                       vk_surface_color_space_, vk_gpu_memory_properties_);

  const vk::CommandPoolCreateInfo cmd_pool_info = vk::CommandPoolCreateInfo()
      .queueFamilyIndex(vk_graphics_queue_node_index_)
      .flags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

  vk::chk(vk_device_.createCommandPool(&cmd_pool_info, nullptr, &vk_cmd_pool_));

  const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
      .commandPool(vk_cmd_pool_)
      .level(vk::CommandBufferLevel::ePrimary)
      .commandBufferCount(1);

  vk::chk(vk_device_.allocateCommandBuffers(&cmd, &vk_draw_cmd_));

  PrepareBuffers();

  vk_depth_buffer_ = CreateDepthBuffer(window, vk_device_, *this);
}

/******************************************************
*                          Dtor                       *
*******************************************************/
VulkanScene::~VulkanScene() {
  if (vk_setup_cmd_) {
    vk_device_.freeCommandBuffers(vk_cmd_pool_, 1, &vk_setup_cmd_);
  }
  vk_device_.freeCommandBuffers(vk_cmd_pool_, 1, &vk_draw_cmd_);
  vk_device_.destroyCommandPool(vk_cmd_pool_, nullptr);

  vk_device_.destroyImageView(vk_depth_buffer_.view, nullptr);
  vk_device_.destroyImage(vk_depth_buffer_.image, nullptr);
  vk_device_.freeMemory(vk_depth_buffer_.mem, nullptr);

  for (uint32_t i = 0; i < vk_swapchain_image_count_; i++) {
    vk_device_.destroyImageView(vk_buffers()[i].view, nullptr);
  }

  vk_app_.entry_points.DestroySwapchainKHR(vk_device_, vk_swapchain_, nullptr);

  vk_device_.destroy(nullptr);
  vk_instance_.destroySurfaceKHR(vk_surface_, nullptr);
}

/******************************************************
*                      ScreenResizedClean                 *
*******************************************************/
void VulkanScene::ScreenResizedClean() {
  if (vk_setup_cmd_) {
    vk_device_.freeCommandBuffers(vk_cmd_pool_, 1, &vk_setup_cmd_);
    vk_setup_cmd_ = VK_NULL_HANDLE;
  }
  vk_device_.freeCommandBuffers(vk_cmd_pool_, 1, &vk_draw_cmd_);
  vk_device_.destroyCommandPool(vk_cmd_pool_, nullptr);

  vk_device_.destroyImageView(vk_depth_buffer_.view, nullptr);
  vk_device_.destroyImage(vk_depth_buffer_.image, nullptr);
  vk_device_.freeMemory(vk_depth_buffer_.mem, nullptr);

  for (uint32_t i = 0; i < vk_swapchain_image_count_; i++) {
    vk_device_.destroyImageView(vk_buffers()[i].view, nullptr);
  }
}

/******************************************************
*                      ScreenResized                 *
*******************************************************/
void VulkanScene::ScreenResized(size_t width, size_t height) {
  const vk::CommandPoolCreateInfo cmd_pool_info = vk::CommandPoolCreateInfo()
      .queueFamilyIndex(vk_graphics_queue_node_index_)
      .flags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

  vk::chk(vk_device_.createCommandPool(&cmd_pool_info, nullptr, &vk_cmd_pool_));

  const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
      .commandPool(vk_cmd_pool_)
      .level(vk::CommandBufferLevel::ePrimary)
      .commandBufferCount(1);

  vk::chk(vk_device_.allocateCommandBuffers(&cmd, &vk_draw_cmd_));

  PrepareBuffers();
  vk_depth_buffer_ = CreateDepthBuffer(window(), vk_device_, *this);
}

/******************************************************
*                      SetImageLayout                 *
*******************************************************/
void VulkanScene::SetImageLayout(const vk::Image& image,
                                 const vk::ImageAspectFlags& aspectMask,
                                 const vk::ImageLayout& old_image_layout,
                                 const vk::ImageLayout& new_image_layout,
                                 vk::AccessFlags src_access) {
    if (vk_setup_cmd_ == VK_NULL_HANDLE) {
        const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
            .commandPool(vk_cmd_pool_)
            .level(vk::CommandBufferLevel::ePrimary)
            .commandBufferCount(1);

        vk::chk(vk_device_.allocateCommandBuffers(&cmd, &vk_setup_cmd_));

        vk::CommandBufferInheritanceInfo cmd_buf_inh_info =
            vk::CommandBufferInheritanceInfo();

        vk::CommandBufferBeginInfo cmd_buf_info =
          vk::CommandBufferBeginInfo().pInheritanceInfo(&cmd_buf_inh_info);

        vk::chk(vk_setup_cmd_.begin(&cmd_buf_info));
    }

    vk::ImageMemoryBarrier image_memory_barrier = vk::ImageMemoryBarrier()
        .oldLayout(old_image_layout)
        .newLayout(new_image_layout)
        .image(image)
        .srcAccessMask(src_access)
        .subresourceRange({aspectMask, 0, 1, 0, 1});

    if (new_image_layout == vk::ImageLayout::eTransferSrcOptimal) {
        /* Make sure anything that was copying from this image has completed */
        image_memory_barrier.dstAccessMask(vk::AccessFlagBits::eTransferRead);
    }

    if (new_image_layout == vk::ImageLayout::eTransferDstOptimal) {
        /* Make sure anything that was copying from this image has completed */
        image_memory_barrier.dstAccessMask(vk::AccessFlagBits::eTransferWrite);
    }

    if (new_image_layout == vk::ImageLayout::eColorAttachmentOptimal) {
        image_memory_barrier.dstAccessMask(
            vk::AccessFlagBits::eColorAttachmentWrite);
    }

    if (new_image_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        image_memory_barrier.dstAccessMask(
            vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    }

    if (new_image_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        /* Make sure any Copy or CPU writes to image are flushed */
        image_memory_barrier.dstAccessMask(vk::AccessFlagBits::eShaderRead |
            vk::AccessFlagBits::eInputAttachmentRead);
    }

    vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eTopOfPipe;

    vk_setup_cmd_.pipelineBarrier(src_stages, dest_stages,
      vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
}


/******************************************************
*                  GET_INSTANCE_PROC_ADDR             *
*******************************************************/
#define GET_INSTANCE_PROC_ADDR(instance, app, entrypoint) {                                \
  app.entry_points.entrypoint =                                                            \
    (PFN_vk##entrypoint)instance.getProcAddr("vk" #entrypoint);                            \
  if (app.entry_points.entrypoint == nullptr) {                                            \
    throw std::runtime_error("vk::getInstanceProcAddr failed to find vk" #entrypoint);     \
  }                                                                                        \
}


/******************************************************
*                      CreateInstance                 *
*******************************************************/
vk::Instance VulkanScene::CreateInstance(VulkanApplication& app) {
  /* Look for instance validation layers */
  uint32_t all_instance_layer_count = 0;
  vk::chk(vk::enumerateInstanceLayerProperties(&all_instance_layer_count, nullptr));

  if (all_instance_layer_count > 0) {
    std::unique_ptr<vk::LayerProperties> instance_layers{
        new vk::LayerProperties[all_instance_layer_count]};
    vk::chk(vk::enumerateInstanceLayerProperties(&all_instance_layer_count,
                                                 instance_layers.get()));

#if VK_VALIDATE
      CheckForMissingLayers(app.instance_validation_layers.size(),
                            app.instance_validation_layers.data(),
                            all_instance_layer_count,
                            instance_layers.get());
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
    app.instance_extension_names.push_back(required_extensions[i]);
  }

  vk::InstanceCreateInfo inst_info = vk::InstanceCreateInfo()
      .pApplicationInfo(&app.application_info)
      .enabledLayerCount(app.instance_validation_layers.size())
      .ppEnabledLayerNames((const char* const*)app.instance_validation_layers.data())
      .enabledExtensionCount(app.instance_extension_names.size())
      .ppEnabledExtensionNames((const char* const*)app.instance_extension_names.data());

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
vk::PhysicalDevice VulkanScene::CreatePhysicalDevice(vk::Instance& instance,
                                                     const VulkanApplication& app) {
  /* Make initial call to query gpu_count, then second call for gpu info*/
  uint32_t gpu_count;
  vk::chk(instance.enumeratePhysicalDevices(&gpu_count, nullptr));

  vk::PhysicalDevice gpu_to_use;
  if (gpu_count > 0) {
      std::unique_ptr<vk::PhysicalDevice> physical_devices{
          new vk::PhysicalDevice[gpu_count]};
      vk::chk(instance.enumeratePhysicalDevices(&gpu_count, physical_devices.get()));

      /* TODO */
      gpu_to_use = physical_devices.get()[0];
  } else {
      throw std::runtime_error("vk::enumeratePhysicalDevices reported zero accessible devices."
                               "\n\nDo you have a compatible Vulkan installable client"
                               " driver (ICD) installed?");
  }

  /* Look for validation layers */
  uint32_t device_layer_count = 0;
  vk::chk(gpu_to_use.enumerateDeviceLayerProperties(&device_layer_count, nullptr));

  if (device_layer_count > 0) {
    std::unique_ptr<vk::LayerProperties> device_layers{
        new vk::LayerProperties[device_layer_count]};
    vk::chk(gpu_to_use.enumerateDeviceLayerProperties(&device_layer_count,
                                                    device_layers.get()));

#if VK_VALIDATE
      CheckForMissingLayers(app.device_validation_layers.size(),
                            app.device_validation_layers.data(),
                            device_layer_count,
                            device_layers.get());
#endif
  }

  return gpu_to_use;
}



/******************************************************
*                   CreateSurface                     *
*******************************************************/
VkSurfaceKHR VulkanScene::CreateSurface(const vk::Instance& instance,
                                        GLFWwindow* window) {

  VkSurfaceKHR surface;

  // Create a WSI surface for the window:
  glfwCreateWindowSurface(instance, window, nullptr, &surface);

  return surface;
}


/******************************************************
*             SelectQraphicsQueueNodeIndex            *
*******************************************************/
uint32_t VulkanScene::SelectQraphicsQueueNodeIndex(const vk::PhysicalDevice& gpu,
                                                   const VkSurfaceKHR& surface,
                                                   const VulkanApplication& app) {
  // Get queue count and properties
  uint32_t queue_count = 0;
  gpu.getQueueFamilyProperties(&queue_count, nullptr);

  std::vector<vk::QueueFamilyProperties> queue_props;
  queue_props.resize(queue_count);
  gpu.getQueueFamilyProperties(&queue_count, queue_props.data());
  assert(1 <= queue_count);
  queue_props.resize(queue_count);

  // Graphics queue and MemMgr queue can be separate.
  // TODO: Add support for separate queues, including synchronization,
  //       and appropriate tracking for QueueSubmit

  // Iterate over each queue to learn whether it supports presenting:
  std::unique_ptr<vk::Bool32> supports_present{new vk::Bool32[queue_count]};
  for (uint32_t i = 0; i < queue_count; i++) {
    app.entry_points.GetPhysicalDeviceSurfaceSupportKHR(
        gpu, i, surface, &supports_present.get()[i]);
  }

  // Search for a graphics and a present queue in the array of queue
  // families, try to find one that supports both
  uint32_t graphics_queue_node_index = UINT32_MAX;
  uint32_t present_queue_node_index = UINT32_MAX;
  for (uint32_t i = 0; i < queue_count; i++) {
    if ((queue_props[i].queueFlags() & vk::QueueFlagBits::eGraphics)
        != vk::QueueFlags{}) {
      if (graphics_queue_node_index == UINT32_MAX) {
        graphics_queue_node_index = i;
      }

      if (supports_present.get()[i]) {
        graphics_queue_node_index = i;
        present_queue_node_index = i;
        break;
      }
    }
  }

  if (present_queue_node_index == UINT32_MAX) {
    // If didn't find a queue that supports both graphics and present, then
    // find a separate present queue.
    for (uint32_t i = 0; i < queue_count; ++i) {
      if (supports_present.get()[i]) {
        present_queue_node_index = i;
        break;
      }
    }
  }

  // Generate error if could not find both a graphics and a present queue
  if (graphics_queue_node_index == UINT32_MAX || present_queue_node_index == UINT32_MAX) {
    throw std::runtime_error("Could not find a graphics and a present queue");
  }

  // TODO: Add support for separate queues, including presentation,
  //       synchronization, and appropriate tracking for QueueSubmit.
  // NOTE: While it is possible for an application to use a separate graphics
  //       and a present queues, this demo program assumes it is only using
  //       one:
  if (graphics_queue_node_index != present_queue_node_index) {
    throw std::runtime_error("Could not find a common graphics and a present queue");
  }

  return graphics_queue_node_index;
}


/******************************************************
*                   GET_DEVICE_PROC_ADDR              *
*******************************************************/
#define GET_DEVICE_PROC_ADDR(device, app, entrypoint) {                                  \
  app.entry_points.entrypoint =                                                          \
    (PFN_vk##entrypoint)device.getProcAddr("vk" #entrypoint);                            \
  if (app.entry_points.entrypoint == nullptr) {                                          \
    throw std::runtime_error("vk::getDeviceProcAddr failed to find vk" #entrypoint);     \
  }                                                                                      \
}

/******************************************************
*                   CreateDevice                      *
*******************************************************/
vk::Device VulkanScene::CreateDevice(const vk::PhysicalDevice& gpu,
                                     uint32_t graphics_queue_node_index,
                                     VulkanApplication& app) {
  float queue_priorities[1] = {0.0};
  const vk::DeviceQueueCreateInfo queue = vk::DeviceQueueCreateInfo()
      .queueFamilyIndex(graphics_queue_node_index)
      .queueCount(1)
      .pQueuePriorities(queue_priorities);

  vk::PhysicalDeviceFeatures features = vk::PhysicalDeviceFeatures()
      .fillModeNonSolid(true);

  vk::DeviceCreateInfo device_create_info = vk::DeviceCreateInfo()
      .queueCreateInfoCount(1)
      .pQueueCreateInfos(&queue)
      .enabledLayerCount(app.device_validation_layers.size())
      .ppEnabledLayerNames(app.device_validation_layers.data())
      .enabledExtensionCount(app.device_extension_names.size())
      .ppEnabledExtensionNames(app.device_extension_names.data())
      .pEnabledFeatures(&features);

  vk::Device device;
  vk::chk(gpu.createDevice(&device_create_info, nullptr, &device));

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
vk::Queue VulkanScene::GetQueue(const vk::Device& device,
                                uint32_t graphics_queue_node_index) {
  vk::Queue queue;
  device.getQueue(graphics_queue_node_index, 0, &queue);
  return queue;
}


/******************************************************
*                  GetSurfaceProperties               *
*******************************************************/
void VulkanScene::GetSurfaceProperties(const vk::PhysicalDevice& gpu,
                                       const VkSurfaceKHR& surface,
                                       const VulkanApplication& app,
                                       vk::Format& format,
                                       vk::ColorSpaceKHR& color_space,
                                       vk::PhysicalDeviceMemoryProperties& memory_properties) {

    // Get the list of vk::Format's that are supported:
    uint32_t format_count;
    VkChk(app.entry_points.GetPhysicalDeviceSurfaceFormatsKHR(
          gpu, surface, &format_count, nullptr));

    std::unique_ptr<VkSurfaceFormatKHR> surf_formats{new VkSurfaceFormatKHR[format_count]};
    VkChk(app.entry_points.GetPhysicalDeviceSurfaceFormatsKHR(
        gpu, surface, &format_count, surf_formats.get()));

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (format_count == 1 && surf_formats.get()[0].format == VK_FORMAT_UNDEFINED) {
        format = vk::Format::eB8G8R8A8Unorm;
    } else {
        assert(format_count >= 1);
        format = static_cast<vk::Format>(surf_formats.get()[0].format);
    }
    color_space = static_cast<vk::ColorSpaceKHR>(surf_formats.get()[0].colorSpace);

    // Get Memory information and properties
    gpu.getMemoryProperties(&memory_properties);
}


/******************************************************
*                  GetSurfaceProperties               *
*******************************************************/
VulkanScene::DepthBuffer VulkanScene::CreateDepthBuffer(GLFWwindow* window,
                                                        const vk::Device& vk_device,
                                                        VulkanScene& scene) {
  DepthBuffer depth;

  const vk::Format depth_format = vk::Format::eD16Unorm;
  const vk::ImageCreateInfo image = vk::ImageCreateInfo()
      .imageType(vk::ImageType::e2D)
      .format(depth_format)
      .extent(vk::Extent3D(scene.framebuffer_size().x, scene.framebuffer_size().y, 1))
      .mipLevels(1)
      .arrayLayers(1)
      .samples(vk::SampleCountFlagBits::e1)
      .tiling(vk::ImageTiling::eOptimal)
      .usage(vk::ImageUsageFlagBits::eDepthStencilAttachment);

  vk::MemoryAllocateInfo mem_alloc;
  vk::ImageViewCreateInfo view = vk::ImageViewCreateInfo()
    .format(depth_format)
    .subresourceRange(vk::ImageSubresourceRange()
      .aspectMask(vk::ImageAspectFlagBits::eDepth)
      .baseMipLevel(0)
      .levelCount(1)
      .baseArrayLayer(0)
      .layerCount(1)
    )
    .viewType(vk::ImageViewType::e2D);

  vk::MemoryRequirements mem_reqs;

  depth.format = depth_format;

  /* create image */
  vk::chk(vk_device.createImage(&image, nullptr, &depth.image));

  /* get memory requirements for this object */
  vk_device.getImageMemoryRequirements(depth.image, &mem_reqs);

  /* select memory size and type */
  mem_alloc.allocationSize(mem_reqs.size());
  MemoryTypeFromProperties(scene.vk_gpu_memory_properties(),
                           mem_reqs.memoryTypeBits(),
                           vk::MemoryPropertyFlags(), /* No requirements */
                           mem_alloc);

  /* allocate memory */
  vk::chk(vk_device.allocateMemory(&mem_alloc, nullptr, &depth.mem));

  /* bind memory */
  vk::chk(vk_device.bindImageMemory(depth.image, depth.mem, 0));

  scene.SetImageLayout(depth.image, vk::ImageAspectFlagBits::eDepth,
                       vk::ImageLayout::eUndefined,
                       vk::ImageLayout::eDepthStencilAttachmentOptimal,
                       vk::AccessFlags{});

  /* create image view */
  view.image(depth.image);
  vk::chk(vk_device.createImageView(&view, nullptr, &depth.view));

  return depth;
}


void VulkanScene::FlushInitCommand() {
  if (vk_setup_cmd_ == VK_NULL_HANDLE)
    return;

  vk::chk(vk_setup_cmd_.end());

  const vk::CommandBuffer commandBuffers[] = {vk_setup_cmd_};
  vk::Fence null_fence = {VK_NULL_HANDLE};
  vk::SubmitInfo submitInfo = vk::SubmitInfo()
                               .commandBufferCount(1)
                               .pCommandBuffers(commandBuffers);

  vk::chk(vk_queue_.submit(1, &submitInfo, null_fence));
  vk::chk(vk_queue_.waitIdle());

  vk_device_.freeCommandBuffers(vk_cmd_pool_, 1, commandBuffers);
  vk_setup_cmd_ = VK_NULL_HANDLE;
}


void VulkanScene::PrepareBuffers() {
  vk::SwapchainKHR old_swapchain = vk_swapchain_;

  // Check the surface capabilities and formats
  vk::SurfaceCapabilitiesKHR surf_capabilities;
  VkSurfaceCapabilitiesKHR& vk_surf_capabilities =
      const_cast<VkSurfaceCapabilitiesKHR&>(
        static_cast<const VkSurfaceCapabilitiesKHR&>(surf_capabilities));
  VkChk(vk_app_.entry_points.GetPhysicalDeviceSurfaceCapabilitiesKHR(
      vk_gpu_, vk_surface_, &vk_surf_capabilities));

  uint32_t present_mode_count;
  VkChk(vk_app_.entry_points.GetPhysicalDeviceSurfacePresentModesKHR(
      vk_gpu_, vk_surface_, &present_mode_count, nullptr));

  std::unique_ptr<VkPresentModeKHR> presentModes{
    new VkPresentModeKHR[present_mode_count]};

  VkChk(vk_app_.entry_points.GetPhysicalDeviceSurfacePresentModesKHR(
        vk_gpu_, vk_surface_, &present_mode_count, presentModes.get()));

  vk::Extent2D swapchain_extent;
  // width and height are either both -1, or both not -1.
  if (surf_capabilities.currentExtent().width() == (uint32_t)-1) {
      // If the surface size is undefined, the size is set to
      // the size of the images requested.
      swapchain_extent.width(framebuffer_size_.x);
      swapchain_extent.height(framebuffer_size_.y);
  } else {
      // If the surface size is defined, the swap chain size must match
      swapchain_extent = surf_capabilities.currentExtent();
      framebuffer_size_.x = surf_capabilities.currentExtent().width();
      framebuffer_size_.y = surf_capabilities.currentExtent().height();
  }

#if VK_VSYNC
  vk::PresentModeKHR swapchain_present_mode = vk::PresentModeKHR::eFifoKHR;
#else
  vk::PresentModeKHR swapchain_present_mode = vk::PresentModeKHR::eImmediateKHR;
#endif

  // Determine the number of vk::Image's to use in the swap chain (we desire to
  // own only 1 image at a time, besides the images being displayed and
  // queued for display):
  uint32_t desired_number_of_swapchain_images = surf_capabilities.minImageCount() + 1;
  if ((surf_capabilities.maxImageCount() > 0) &&
      (desired_number_of_swapchain_images > surf_capabilities.maxImageCount())) {
      // Application must settle for fewer images than desired:
      desired_number_of_swapchain_images = surf_capabilities.maxImageCount();
  }

  vk::SurfaceTransformFlagBitsKHR pre_transform;
  if (surf_capabilities.supportedTransforms() & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
      pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
  } else {
      pre_transform = surf_capabilities.currentTransform();
  }

  const vk::SwapchainCreateInfoKHR swapchain_info = vk::SwapchainCreateInfoKHR()
      .surface(vk_surface_)
      .minImageCount(desired_number_of_swapchain_images)
      .imageFormat(vk_surface_format_)
      .imageColorSpace(vk_surface_color_space_)
      .imageExtent(vk::Extent2D{swapchain_extent.width(), swapchain_extent.height()})
      .imageUsage(vk::ImageUsageFlagBits::eColorAttachment)
      .preTransform(pre_transform)
      .compositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
      .imageArrayLayers(1)
      .imageSharingMode(vk::SharingMode::eExclusive)
      .queueFamilyIndexCount(0)
      .pQueueFamilyIndices(nullptr)
      .presentMode(swapchain_present_mode)
      .oldSwapchain(old_swapchain)
      .clipped(true);

  const VkSwapchainCreateInfoKHR& vk_swapchain_info = swapchain_info;
  VkSwapchainKHR vk_swapchain = vk_swapchain_;
  VkChk(vk_app_.entry_points.CreateSwapchainKHR(
      vk_device_, &vk_swapchain_info, nullptr, &vk_swapchain));
  vk_swapchain_ = vk_swapchain;

  // If we just re-created an existing swapchain, we should destroy the old
  // swapchain at this point.
  // Note: destroying the swapchain also cleans up all its associated
  // presentable images once the platform is done with them.
  if (old_swapchain != VK_NULL_HANDLE) {
      vk_app_.entry_points.DestroySwapchainKHR(vk_device_, old_swapchain, nullptr);
  }

  VkChk(vk_app_.entry_points.GetSwapchainImagesKHR(
       vk_device_, vk_swapchain_, &vk_swapchain_image_count_, nullptr));

  VkImage *swapchainImages = new VkImage[vk_swapchain_image_count_];
  VkChk(vk_app_.entry_points.GetSwapchainImagesKHR(
      vk_device_, vk_swapchain_, &vk_swapchain_image_count_, swapchainImages));

  vk_buffers_ = std::unique_ptr<SwapchainBuffers>(
      new SwapchainBuffers[vk_swapchain_image_count_]);

  for (uint32_t i = 0; i < vk_swapchain_image_count_; i++) {
      vk::ImageViewCreateInfo color_attachment_view = vk::ImageViewCreateInfo()
          .format(vk_surface_format_)
          .subresourceRange(vk::ImageSubresourceRange()
            .aspectMask(vk::ImageAspectFlagBits::eColor)
            .baseMipLevel(0)
            .levelCount(1)
            .baseArrayLayer(0)
            .layerCount(1)
          )
          .viewType(vk::ImageViewType::e2D);

      vk_buffers()[i].image = swapchainImages[i];

      // Render loop will expect image to have been used before and in
      // vk::ImageLayout::ePresentSrcKHR
      // layout and will change to COLOR_ATTACHMENT_OPTIMAL, so init the image
      // to that state
      SetImageLayout(vk_buffers()[i].image,
                     vk::ImageAspectFlagBits::eColor,
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::ePresentSrcKHR,
                     vk::AccessFlags{});

      color_attachment_view.image(vk_buffers()[i].image);

      vk_device_.createImageView(&color_attachment_view, nullptr,
                                &vk_buffers()[i].view);
  }

  vk_current_buffer_ = 0;
}


#if VK_VALIDATE

/******************************************************
*                  CheckForMissingLayers              *
*******************************************************/
void VulkanScene::CheckForMissingLayers(uint32_t check_count,
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
