#include "engine/scene.hpp"

#include <stdexcept>
#include "engine/game_engine.hpp"

namespace engine {

Scene::Scene(GLFWwindow *window)
    : Behaviour(nullptr)
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
}

Scene::~Scene() {
  vkDevice_.destroy(nullptr);
  vkInstance_.destroySurfaceKHR(vkSurface_, nullptr);
}

void Scene::keyAction(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_F1:
        // camera_time_.toggle();
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
