// Copyright (c) 2015, Tamas Csala

#ifndef ENGINE_GAME_ENGINE_H_
#define ENGINE_GAME_ENGINE_H_

#include <typeinfo>
#include "engine/scene.hpp"

// #define ENGINE_NO_FULLSCREEN 1

namespace engine {

class GameEngine {
 public:
  GameEngine();
  ~GameEngine();

  void LoadScene(std::unique_ptr<Scene>&& new_scene);
  Scene* scene() { return scene_.get(); }
  GLFWwindow* window() { return window_; }
  glm::vec2 window_size();
  void Run();

 private:
  std::unique_ptr<Scene> scene_ = nullptr;
  std::unique_ptr<Scene> new_scene_ = nullptr;
  GLFWwindow *window_ = nullptr;

  // Callbacks
  static void ErrorCallback(int error, const char* message) {
    std::cerr << "GLFW error: " << message << std::endl;
  }

  static void KeyCallback(GLFWwindow* window, int key, int scancode,
                          int action, int mods);

  static void CharCallback(GLFWwindow* window, unsigned codepoint) {
    GameEngine* gameEngine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
    if (gameEngine && gameEngine->scene_) {
      gameEngine->scene_->charTypedAll(codepoint);
    }
  }

  static void ScreenResizeCallback(GLFWwindow* window, int width, int height) {
    GameEngine* gameEngine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
    if (gameEngine && gameEngine->scene_) {
      std::cout << "Screen resized to " << width << "x" << height << "." << std::endl;
      gameEngine->scene_->screenResizedAll(width, height);
    }
  }

  static void MouseScrolledCallback(GLFWwindow* window, double xoffset,
                                    double yoffset) {
    GameEngine* gameEngine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
    if (gameEngine && gameEngine->scene_) {
      gameEngine->scene_->mouseScrolledAll(xoffset, yoffset);
    }
  }

  static void MouseButtonPressed(GLFWwindow* window, int button,
                                 int action, int mods) {
    GameEngine* gameEngine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
    if (gameEngine && gameEngine->scene_) {
      gameEngine->scene_->mouseButtonPressedAll(button, action, mods);
    }
  }

  static void MouseMoved(GLFWwindow* window, double xpos, double ypos) {
    GameEngine* gameEngine = reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(window));
    if (gameEngine && gameEngine->scene_) {
      gameEngine->scene_->mouseMovedAll(xpos, ypos);
    }
  }
};

}  // namespace engine

#endif
