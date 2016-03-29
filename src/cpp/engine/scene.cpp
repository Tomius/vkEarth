// Copyright (c) 2016, Tamas Csala

#include "engine/scene.hpp"

#include <stdexcept>
#include "engine/game_engine.hpp"
#include "common/error_checking.hpp"

namespace engine {

Scene::Scene(GLFWwindow *window)
    : GameObject(nullptr)
    , camera_(nullptr)
    , window_(window)
    , vkInstance_(CreateInstance(vkApp_))
#if VK_VALIDATE
    , vkDebugCallback_(new DebugCallback(vkInstance_))
#endif
    , vkGpu_(CreatePhysicalDevice(vkInstance_, vkApp_))
    , vkSurface_(CreateSurface(vkInstance_, window))
    , vkGraphicsQueueNodeIndex_(
        SelectQraphicsQueueNodeIndex(vkGpu_, vkSurface_, vkApp_))
    , vkDevice_(CreateDevice(vkGpu_, vkGraphicsQueueNodeIndex_, vkApp_))
    , vkQueue_(GetQueue(vkDevice_, vkGraphicsQueueNodeIndex_)) {
  set_scene(this);

  GetSurfaceProperties(vkGpu_, vkSurface_, vkApp_, vkSurfaceFormat_,
                       vkSurfaceColorSpace_, vkGpuMemoryProperties_);

  const vk::CommandPoolCreateInfo cmdPoolInfo = vk::CommandPoolCreateInfo()
      .queueFamilyIndex(vkGraphicsQueueNodeIndex_)
      .flags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

  vk::chk(vkDevice_.createCommandPool(&cmdPoolInfo, nullptr, &vkCmdPool_));

  const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
      .commandPool(vkCmdPool_)
      .level(vk::CommandBufferLevel::ePrimary)
      .commandBufferCount(1);

  vk::chk(vkDevice_.allocateCommandBuffers(&cmd, &vkDrawCmd_));

  PrepareBuffers();

  vkDepthBuffer_ = CreateDepthBuffer(window_, vkDevice_, *this);
}

Scene::~Scene() {
  if (vkSetupCmd_) {
    vkDevice_.freeCommandBuffers(vkCmdPool_, 1, &vkSetupCmd_);
  }
  vkDevice_.freeCommandBuffers(vkCmdPool_, 1, &vkDrawCmd_);
  vkDevice_.destroyCommandPool(vkCmdPool_, nullptr);

  vkDevice_.destroyImageView(vkDepthBuffer_.view, nullptr);
  vkDevice_.destroyImage(vkDepthBuffer_.image, nullptr);
  vkDevice_.freeMemory(vkDepthBuffer_.mem, nullptr);

  for (uint32_t i = 0; i < vkSwapchainImageCount_; i++) {
    vkDevice_.destroyImageView(vkBuffers()[i].view, nullptr);
  }

  vkApp_.entry_points.DestroySwapchainKHR(vkDevice_, vkSwapchain_, nullptr);

  vkDevice_.destroy(nullptr);
  vkInstance_.destroySurfaceKHR(vkSurface_, nullptr);
}

void Scene::KeyAction(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_F1:
        cameraTime_.toggle();
        break;
      default:
        break;
    }
  }
}

void Scene::turn() {
  UpdateAll();
  RenderAll();
  Render2DAll();
}

void Scene::UpdateAll() {
  cameraTime_.tick();

  GameObject::UpdateAll();
}

void Scene::RenderAll() {
  if (!camera_) {
    throw std::runtime_error("Need a camera to render a 3D scene.");
  }

  GameObject::RenderAll();
}

void Scene::Render2DAll() {
  GameObject::Render2DAll();
}

void Scene::ScreenResizedClean() {
  if (vkSetupCmd_) {
    vkDevice_.freeCommandBuffers(vkCmdPool_, 1, &vkSetupCmd_);
    vkSetupCmd_ = VK_NULL_HANDLE;
  }
  vkDevice_.freeCommandBuffers(vkCmdPool_, 1, &vkDrawCmd_);
  vkDevice_.destroyCommandPool(vkCmdPool_, nullptr);

  vkDevice_.destroyImageView(vkDepthBuffer_.view, nullptr);
  vkDevice_.destroyImage(vkDepthBuffer_.image, nullptr);
  vkDevice_.freeMemory(vkDepthBuffer_.mem, nullptr);

  for (uint32_t i = 0; i < vkSwapchainImageCount_; i++) {
    vkDevice_.destroyImageView(vkBuffers()[i].view, nullptr);
  }
}

void Scene::ScreenResized(size_t width, size_t height) {
  const vk::CommandPoolCreateInfo cmdPoolInfo = vk::CommandPoolCreateInfo()
      .queueFamilyIndex(vkGraphicsQueueNodeIndex_)
      .flags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

  vk::chk(vkDevice_.createCommandPool(&cmdPoolInfo, nullptr, &vkCmdPool_));

  const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
      .commandPool(vkCmdPool_)
      .level(vk::CommandBufferLevel::ePrimary)
      .commandBufferCount(1);

  vk::chk(vkDevice_.allocateCommandBuffers(&cmd, &vkDrawCmd_));

  PrepareBuffers();
  vkDepthBuffer_ = CreateDepthBuffer(window_, vkDevice_, *this);
}

}  // namespace engine
