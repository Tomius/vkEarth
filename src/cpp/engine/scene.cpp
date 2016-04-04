// Copyright (c) 2016, Tamas Csala

#include "engine/scene.hpp"

#include <stdexcept>
#include "engine/game_engine.hpp"
#include "common/error_checking.hpp"

namespace engine {

Scene::Scene(GLFWwindow *window)
    : GameObject(nullptr)
    , camera_(nullptr)
    , window_(window) {
  set_scene(this);
}

Scene::~Scene() {}

void Scene::KeyAction(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_F1:
        camera_time_.Toggle();
        break;
      case GLFW_KEY_P:
        if (camera()) {
          std::cout << camera()->transform().pos() << std::endl;
        }
        break;
      default:
        break;
    }
  }
}

void Scene::Turn() {
  UpdateAll();
  RenderAll();
  Render2DAll();
}

void Scene::UpdateAll() {
  camera_time_.Tick();

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

}  // namespace engine
