// Copyright (c) 2016, Tamas Csala

#ifndef ENGINE_SCENE_HPP_
#define ENGINE_SCENE_HPP_

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
  virtual ~Scene();

  const Timer& camera_time() const { return camera_time_; }
  Timer& camera_time() { return camera_time_; }

  const Camera* camera() const { return camera_; }
  Camera* camera() { return camera_; }
  void set_camera(Camera* camera) { camera_ = camera; }

  GLFWwindow* window() const { return window_; }
  void set_window(GLFWwindow* window) { window_ = window; }

  virtual void KeyAction(int key, int scancode, int action, int mods) override;
  virtual void Turn();

 private:
  Camera* camera_;
  Timer camera_time_;
  GLFWwindow* window_;

protected:
  virtual void UpdateAll() override;
  virtual void RenderAll() override;
  virtual void Render2DAll() override;
};

}  // namespace engine


#endif
