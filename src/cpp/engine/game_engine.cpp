// Copyright (c) 2016, Tamas Csala

#include <string>
#include <GLFW/glfw3.h>

#include "engine/game_engine.hpp"

namespace engine {

GameEngine::GameEngine() {
  glfwSetErrorCallback(ErrorCallback);

  if (!glfwInit()) {
    std::cerr << "Cannot initialize GLFW.\nExiting ..." << std::endl;
    std::terminate();
  }

  if (!glfwVulkanSupported()) {
    std::cerr << "Cannot find a compatible Vulkan installable client driver "
                 "(ICD).\nExiting ..." << std::endl;
    std::terminate();
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(600, 600, "Vulkan planetary CDLOD",
                             nullptr, nullptr);
  if (!window_) {
    std::cerr << "Cannot create a window in which to draw!" << std::endl;
    std::terminate();
  }

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
  int width = 0, height = 0;
  int last_width = 0, last_height = 0;
  glfwGetWindowSize(window_, &last_width, &last_height);
  while (!glfwWindowShouldClose(window_)) {
    if (new_scene_) {
      std::swap(scene_, new_scene_);
      new_scene_ = nullptr;
    }

    if (scene_) {
      glfwPollEvents();
      scene_->Turn();
    }

    // GLFW bug workaround (the screen resize callback is often not called)
    glfwGetWindowSize(window_, &width, &height);
    if (width != last_width || height != last_height) {
      ScreenResizeCallback(window_, width, height);
      last_width = width;
      last_height = height;
    }
  }
}

void GameEngine::ErrorCallback(int error, const char* message) {
  std::cerr << "GLFW error: " << message << std::endl;
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

  GameEngine* game_engine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (game_engine && game_engine->scene_) {
    game_engine->scene_->KeyActionAll(key, scancode, action, mods);
  }
}

void GameEngine::CharCallback(GLFWwindow* window, unsigned codepoint) {
  GameEngine* game_engine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (game_engine && game_engine->scene_) {
    game_engine->scene_->CharTypedAll(codepoint);
  }
}

void GameEngine::ScreenResizeCallback(GLFWwindow* window, int width, int height) {
  GameEngine* game_engine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (game_engine && game_engine->scene_) {
    std::cout << "Screen resized to " << width << "x" << height << "." << std::endl;
    game_engine->scene_->ScreenResizedCleanAll();
    game_engine->scene_->ScreenResizedAll(width, height);
  }
}

void GameEngine::MouseScrolledCallback(GLFWwindow* window, double xoffset,
                                  double yoffset) {
  GameEngine* game_engine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (game_engine && game_engine->scene_) {
    game_engine->scene_->MouseScrolledAll(xoffset, yoffset);
  }
}

void GameEngine::MouseButtonPressed(GLFWwindow* window, int button,
                               int action, int mods) {
  GameEngine* game_engine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (game_engine && game_engine->scene_) {
    game_engine->scene_->MouseButtonPressedAll(button, action, mods);
  }
}

void GameEngine::MouseMoved(GLFWwindow* window, double xpos, double ypos) {
  GameEngine* game_engine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
  if (game_engine && game_engine->scene_) {
    game_engine->scene_->MouseMovedAll(xpos, ypos);
  }
}

}  // namespace engine
