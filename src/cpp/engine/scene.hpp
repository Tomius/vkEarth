// Copyright (c) 2015, Tamas Csala

#ifndef ENGINE_SCENE_H_
#define ENGINE_SCENE_H_

#include <vector>
#include <memory>
#include <vulkan/vk_cpp.h>

#include "engine/timer.hpp"
#include "engine/camera.hpp"
#include "engine/behaviour.hpp"
#include "engine/game_object.hpp"

#include "common/debug_callback.hpp"
#include "common/vulkan_application.hpp"

namespace engine {

class GameObject;

class Scene : public Behaviour {
 public:
  Scene(GLFWwindow *window);
  ~Scene();

  const Timer& camera_time() const { return camera_time_; }
  Timer& camera_time() { return camera_time_; }

  const Camera* camera() const { return camera_; }
  Camera* camera() { return camera_; }
  void set_camera(Camera* camera) { camera_ = camera; }

  GLFWwindow* window() const { return window_; }
  void set_window(GLFWwindow* window) { window_ = window; }

  virtual void keyAction(int key, int scancode, int action, int mods) override;
  virtual void turn();

  const vk::Queue& vkQueue() const { return vkQueue_; }
  const vk::Device& vkDevice() const { return vkDevice_; }
  const VkSurfaceKHR& vkSurface() const { return vkSurface_; }
  const VulkanApplication& vkApp() const { return vkApp_; }
  const vk::PhysicalDevice& vkGpu() const { return vkGpu_; }
  const vk::Format& vkSurfaceFormat() const { return vkSurfaceFormat_; }
  uint32_t vkGraphicsQueueNodeIndex() const { return vkGraphicsQueueNodeIndex_; }
  const vk::ColorSpaceKHR& vkSurfaceColorSpace() const { return vkSurfaceColorSpace_; }
  const vk::PhysicalDeviceMemoryProperties& vkGpuMemoryProperties() const { return vkGpuMemoryProperties_; }

 private:
  Camera* camera_;
  Timer camera_time_;
  GLFWwindow* window_;

  // private vulkan stuff (no getters)
  VulkanApplication vkApp_;
  vk::Instance vkInstance_;
#if VK_VALIDATE
  std::unique_ptr<DebugCallback> vkDebugCallback_;
#endif

  // "public" vulkan stuff (through getters)
  vk::PhysicalDevice vkGpu_;
  VkSurfaceKHR vkSurface_;
  uint32_t vkGraphicsQueueNodeIndex_ = 0;
  vk::Device vkDevice_;
  vk::Queue vkQueue_;

  vk::Format vkSurfaceFormat_;
  vk::ColorSpaceKHR vkSurfaceColorSpace_;
  vk::PhysicalDeviceMemoryProperties vkGpuMemoryProperties_;

  virtual void updateAll() override;
  virtual void renderAll() override;
  virtual void render2DAll() override;

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

#if VK_VALIDATE
  static void CheckForMissingLayers(uint32_t check_count,
                                    const char* const* check_names,
                                    uint32_t layer_count,
                                    vk::LayerProperties* layers);
#endif // VK_VALIDATE

};

}  // namespace engine


#endif
