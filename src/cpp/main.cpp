// Copyright (c) 2016, Tamas Csala

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "engine/game_engine.hpp"
#include "demo_scene.hpp"

int main(const int argc, const char *argv[]) {
  engine::GameEngine engine;
  engine.LoadScene(std::unique_ptr<engine::Scene>{new DemoScene(engine.window())});
  engine.Run();

  return 0;
}

