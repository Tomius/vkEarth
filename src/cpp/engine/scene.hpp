// Copyright (c) 2016, Tamas Csala

#ifndef ENGINE_SCENE_H_
#define ENGINE_SCENE_H_

#include <vector>
#include <memory>
#include <vulkan/vk_cpp.h>

#include "engine/timer.hpp"
#include "engine/camera.hpp"
#include "engine/game_object.hpp"

#include "common/debug_callback.hpp"
#include "common/vulkan_application.hpp"

namespace engine {

class GameObject;

class Scene : public GameObject {
 public:
  Scene(GLFWwindow *window);
  ~Scene();

  const Timer& cameraTime() const { return cameraTime_; }
  Timer& cameraTime() { return cameraTime_; }

  const Camera* camera() const { return camera_; }
  Camera* camera() { return camera_; }
  void setCamera(Camera* camera) { camera_ = camera; }

  GLFWwindow* window() const { return window_; }
  void setWindow(GLFWwindow* window) { window_ = window; }

  virtual void KeyAction(int key, int scancode, int action, int mods) override;
  virtual void turn();

  const vk::Queue& vkQueue() const { return vkQueue_; }
  const vk::Device& vkDevice() const { return vkDevice_; }
  const VkSurfaceKHR& vkSurface() const { return vkSurface_; }
  const VulkanApplication& vkApp() const { return vkApp_; }
  const vk::PhysicalDevice& vkGpu() const { return vkGpu_; }
  const vk::Format& vkSurfaceFormat() const { return vkSurfaceFormat_; }
  const vk::PhysicalDeviceMemoryProperties& vkGpuMemoryProperties() const { return vkGpuMemoryProperties_; }
  const vk::CommandBuffer& vkSetupCmd() const { return vkSetupCmd_; }
  const vk::CommandBuffer& vkDrawCmd() const { return vkDrawCmd_; }

  struct SwapchainBuffers {
    vk::Image image;
    vk::CommandBuffer cmd;
    vk::ImageView view;
  };

  glm::ivec2 framebufferSize() const { return framebufferSize_; }

  const vk::SwapchainKHR& vkSwapchain() const { return vkSwapchain_; }
  uint32_t vkSwapchainImageCount() const { return vkSwapchainImageCount_; }
  const SwapchainBuffers* vkBuffers() const { return vkBuffers_.get(); }
  SwapchainBuffers* vkBuffers() { return vkBuffers_.get(); }
  uint32_t& vkCurrentBuffer() { return vkCurrentBuffer_; }

  struct DepthBuffer {
    vk::Format format;

    vk::Image image;
    vk::DeviceMemory mem;
    vk::ImageView view;
  };

  const DepthBuffer& vkDepthBuffer() const { return vkDepthBuffer_; }

  void SetImageLayout(const vk::Image& image,
                      const vk::ImageAspectFlags& aspectMask,
                      const vk::ImageLayout& oldImageLayout,
                      const vk::ImageLayout& newImageLayout,
                      vk::AccessFlags srcAccess);

  void FlushInitCommand();

 private:
  Camera* camera_;
  Timer cameraTime_;
  GLFWwindow* window_;

  VulkanApplication vkApp_;
  vk::Instance vkInstance_;
#if VK_VALIDATE
  std::unique_ptr<DebugCallback> vkDebugCallback_;
#endif

  vk::PhysicalDevice vkGpu_;
  VkSurfaceKHR vkSurface_;
  uint32_t vkGraphicsQueueNodeIndex_ = 0;
  vk::Device vkDevice_;
  vk::Queue vkQueue_;

  vk::Format vkSurfaceFormat_;
  vk::ColorSpaceKHR vkSurfaceColorSpace_;
  vk::PhysicalDeviceMemoryProperties vkGpuMemoryProperties_;

  glm::ivec2 framebufferSize_;

  vk::SwapchainKHR vkSwapchain_;
  uint32_t vkSwapchainImageCount_ = 0;
  std::unique_ptr<SwapchainBuffers> vkBuffers_;
  uint32_t vkCurrentBuffer_ = 0;

  vk::CommandPool vkCmdPool_;
  vk::CommandBuffer vkSetupCmd_;
  vk::CommandBuffer vkDrawCmd_;
  DepthBuffer vkDepthBuffer_;

protected:
  virtual void UpdateAll() override;
  virtual void RenderAll() override;
  virtual void Render2DAll() override;
  virtual void ScreenResizedClean() override;
  virtual void ScreenResized(size_t width, size_t height) override;

private:
  static vk::Instance CreateInstance(VulkanApplication& app);

  static vk::PhysicalDevice CreatePhysicalDevice(vk::Instance& instance,
                                                 const VulkanApplication& app);

  static VkSurfaceKHR CreateSurface(const vk::Instance& instance,
                                    GLFWwindow* window);

  static uint32_t SelectQraphicsQueueNodeIndex(const vk::PhysicalDevice& gpu,
                                               const VkSurfaceKHR& surface,
                                               const VulkanApplication& app);

  static vk::Device CreateDevice(const vk::PhysicalDevice& gpu,
                                 uint32_t graphicsQueueNodeIndex,
                                 VulkanApplication& app);

  static vk::Queue GetQueue(const vk::Device& device,
                            uint32_t graphicsQueueNodeIndex);

  static void GetSurfaceProperties(const vk::PhysicalDevice& gpu,
                                   const VkSurfaceKHR& surface,
                                   const VulkanApplication& app,
                                   vk::Format& format,
                                   vk::ColorSpaceKHR& colorSpace,
                                   vk::PhysicalDeviceMemoryProperties& memoryProperties);

  void PrepareBuffers();

  static DepthBuffer CreateDepthBuffer(GLFWwindow* window,
                                       const vk::Device& vkDevice,
                                       Scene& scene);

#if VK_VALIDATE
  static void CheckForMissingLayers(uint32_t check_count,
                                    const char* const* check_names,
                                    uint32_t layer_count,
                                    vk::LayerProperties* layers);
#endif // VK_VALIDATE

};

}  // namespace engine


#endif
