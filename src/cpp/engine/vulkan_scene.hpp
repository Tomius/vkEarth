// Copyright (c) 2016, Tamas Csala

#ifndef ENGINE_VULKAN_SCENE_HPP_
#define ENGINE_VULKAN_SCENE_HPP_

#include <vulkan/vk_cpp.hpp>

#include "engine/scene.hpp"

#include "common/debug_callback.hpp"
#include "common/vulkan_application.hpp"

namespace engine {

class VulkanScene : public engine::Scene {
 public:
  VulkanScene(GLFWwindow *window);
  ~VulkanScene();

  const vk::Queue& vk_queue() const { return vk_queue_; }
  const vk::Device& vk_device() const { return vk_device_; }
  const VkSurfaceKHR& vk_surface() const { return vk_surface_; }
  const VulkanApplication& vk_app() const { return vk_app_; }
  const vk::PhysicalDevice& vk_gpu() const { return vk_gpu_; }
  const vk::Format& vk_surface_format() const { return vk_surface_format_; }
  const vk::PhysicalDeviceMemoryProperties& vk_gpu_memory_properties() const { return vk_gpu_memory_properties_; }
  const vk::CommandBuffer& vk_setup_cmd() const { return vk_setup_cmd_; }
  const vk::CommandBuffer& vk_draw_cmd() const { return vk_draw_cmd_; }

  struct SwapchainBuffers {
    vk::Image image;
    vk::CommandBuffer cmd;
    vk::ImageView view;
  };

  glm::ivec2 framebuffer_size() const { return framebuffer_size_; }

  const vk::SwapchainKHR& vk_swapchain() const { return vk_swapchain_; }
  uint32_t vk_swapchain_image_count() const { return vk_swapchain_image_count_; }
  const SwapchainBuffers* vk_buffers() const { return vk_buffers_.get(); }
  SwapchainBuffers* vk_buffers() { return vk_buffers_.get(); }
  uint32_t& vk_current_buffer() { return vk_current_buffer_; }

  struct DepthBuffer {
    vk::Format format;

    vk::Image image;
    vk::DeviceMemory mem;
    vk::ImageView view;
  };

  const DepthBuffer& vk_depth_buffer() const { return vk_depth_buffer_; }

  void SetImageLayout(const vk::Image& image,
                      const vk::ImageAspectFlags& aspectMask,
                      const vk::ImageLayout& old_image_layout,
                      const vk::ImageLayout& new_image_layout,
                      vk::AccessFlags srcAccess);

  void FlushInitCommand();

 private:
  VulkanApplication vk_app_;
  vk::Instance vk_instance_;
#if VK_VALIDATE
  std::unique_ptr<DebugCallback> vk_debug_callback_;
#endif

  vk::PhysicalDevice vk_gpu_;
  VkSurfaceKHR vk_surface_;
  uint32_t vk_graphics_queue_node_index_ = 0;
  vk::Device vk_device_;
  vk::Queue vk_queue_;

  vk::Format vk_surface_format_;
  vk::ColorSpaceKHR vk_surface_color_space_;
  vk::PhysicalDeviceMemoryProperties vk_gpu_memory_properties_;

  glm::ivec2 framebuffer_size_;

  vk::SwapchainKHR vk_swapchain_;
  uint32_t vk_swapchain_image_count_ = 0;
  std::unique_ptr<SwapchainBuffers> vk_buffers_;
  uint32_t vk_current_buffer_ = 0;

  vk::CommandPool vk_cmd_pool_;
  vk::CommandBuffer vk_setup_cmd_;
  vk::CommandBuffer vk_draw_cmd_;
  DepthBuffer vk_depth_buffer_;

protected:
  virtual void ScreenResizedClean() override;
  virtual void ScreenResized(size_t width, size_t height) override;

private:
  static vk::Instance CreateInstance(VulkanApplication& app);

  static vk::PhysicalDevice CreatePhysicalDevice(vk::Instance& instance,
                                                 VulkanApplication& app);

  static VkSurfaceKHR CreateSurface(const vk::Instance& instance,
                                    GLFWwindow* window);

  static uint32_t SelectQraphicsQueueNodeIndex(const vk::PhysicalDevice& gpu,
                                               const VkSurfaceKHR& surface,
                                               const VulkanApplication& app);

  static vk::Device CreateDevice(const vk::PhysicalDevice& gpu,
                                 uint32_t graphics_queue_node_index,
                                 VulkanApplication& app);

  static vk::Queue GetQueue(const vk::Device& device,
                            uint32_t graphics_queue_node_index);

  static void GetSurfaceProperties(const vk::PhysicalDevice& gpu,
                                   const VkSurfaceKHR& surface,
                                   const VulkanApplication& app,
                                   vk::Format& format,
                                   vk::ColorSpaceKHR& color_space,
                                   vk::PhysicalDeviceMemoryProperties& memory_properties);

  void PrepareBuffers();

  static DepthBuffer CreateDepthBuffer(GLFWwindow* window,
                                       const vk::Device& vk_device,
                                       VulkanScene& scene);

#if VK_VALIDATE
  static void CheckForMissingLayers(std::vector<const char*>& check_names,
                                    uint32_t layer_count,
                                    vk::LayerProperties* layers);
#endif // VK_VALIDATE

};

}  // namespace engine


#endif
