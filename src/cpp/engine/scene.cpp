// Copyright (c) 2015, Tamas Csala

#include "engine/scene.hpp"
#include "engine/game_engine.hpp"

namespace engine {

Scene::Scene(GLFWwindow *window)
    : Behaviour(nullptr)
    , camera_(nullptr)
    , window_(window) {
  set_scene(this);
}

}  // namespace engine
