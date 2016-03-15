// Copyright (c) 2015, Tamas Csala

#include <string>
#include <GLFW/glfw3.h>

#include "engine/game_engine.hpp"

namespace engine {

GameEngine::GameEngine(GLFWwindow *window) : window_ (window) {
  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, KeyCallback);
  glfwSetCharCallback(window_, CharCallback);
  //glfwSetFramebufferSizeCallback(window_, ScreenResizeCallback);
  glfwSetScrollCallback(window_, MouseScrolledCallback);
  glfwSetMouseButtonCallback(window_, MouseButtonPressed);
  glfwSetCursorPosCallback(window_, MouseMoved);
}

GameEngine::~GameEngine() {
  scene_ = nullptr;
  new_scene_ = nullptr;
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

void GameEngine::LoadScene(std::unique_ptr<Scene>&& new_scene) {
  new_scene_ = std::move(new_scene);
}

glm::vec2 GameEngine::window_size() {
  if (window_) {
    int width, height;
    glfwGetWindowSize(window_, &width, &height);
    return glm::vec2(width, height);
  } else {
    return glm::vec2{};
  }
}

void GameEngine::Run() {
  while (!glfwWindowShouldClose(window_)) {
    if (new_scene_) {
      std::swap(scene_, new_scene_);
      new_scene_ = nullptr;
    }

    if (scene_) {
      scene_->turn();
    }

    glfwPollEvents();
  }
}

void GameEngine::KeyCallback(GLFWwindow* window, int key, int scancode,
                             int action, int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
      case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GL_TRUE);
        break;
      case GLFW_KEY_F11: {
        static bool fix_mouse = false;
        fix_mouse = !fix_mouse;

        if (fix_mouse) {
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
          glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
      } break;
      default:
        break;
    }
  }

  GameEngine* gameEngine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (gameEngine && gameEngine->scene_) {
    gameEngine->scene_->keyActionAll(key, scancode, action, mods);
  }
}

}  // namespace engine