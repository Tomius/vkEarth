#include "engine/scene.hpp"

#include <stdexcept>
#include "engine/game_engine.hpp"
#include "initialize/create_physical_device.hpp"
#include "initialize/prepare_swapchain.hpp"

namespace engine {

Scene::Scene(GLFWwindow *window)
    : Behaviour(nullptr)
    , camera_(nullptr)
    , window_(window) {
  set_scene(this);

  vkInstance_ = Initialize::CreateInstance(vkApp_);
#if VK_VALIDATE
  vkDebugCallback_ = std::unique_ptr<DebugCallback>{new DebugCallback(vkInstance_)};
#endif
  vkGpu_  = Initialize::CreatePhysicalDevice(vkInstance_, vkApp_);

  vkSurface_ = Initialize::CreateSurface(vkInstance_, window);
  Initialize::GetSurfaceProperties(vkGpu_, vkSurface_, vkApp_, vkSurfaceFormat_,
                                   vkSurfaceColorSpace_, vkGpuMemoryProperties_);
  vkGraphicsQueueNodeIndex_ = Initialize::SelectQraphicsQueueNodeIndex(
      vkGpu_, vkSurface_, vkApp_);
  vkDevice_ = Initialize::CreateDevice(vkGpu_, vkGraphicsQueueNodeIndex_, vkApp_);
  vkQueue_ = Initialize::GetQueue(vkDevice_, vkGraphicsQueueNodeIndex_);
}

Scene::~Scene() {
  vkDevice_.destroy(nullptr);
  vkInstance_.destroySurfaceKHR(vkSurface_, nullptr);
}

void Scene::keyAction(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_F1:
        game_time_.toggle();
        break;
      case GLFW_KEY_F2:
        environment_time_.toggle();
        break;
      default:
        break;
    }
  }
}

void Scene::turn() {
  updateAll();
  renderAll();
  render2DAll();
}

void Scene::updateAll() {
  game_time_.tick();
  environment_time_.tick();
  camera_time_.tick();

  Behaviour::updateAll();
}

void Scene::renderAll() {
  if (!camera_) {
    throw std::runtime_error("Need a camera to render a 3D scene.");
  }

  Behaviour::renderAll();
}

void Scene::render2DAll() {
  Behaviour::render2DAll();
}

}  // namespace engine
