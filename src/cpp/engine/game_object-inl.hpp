// Copyright (c) 2016, Tamas Csala

#ifndef ENGINE_GAME_OBJECT_INL_HPP_
#define ENGINE_GAME_OBJECT_INL_HPP_

#include <iostream>
#include "engine/game_object.hpp"

namespace engine {

template<typename Transform_t>
GameObject::GameObject(GameObject* parent, const Transform_t& transform)
    : scene_(parent ? parent->scene_ : nullptr), parent_(parent)
    , transform_(new Transform_t{transform})
    , enabled_(true) {
  if (parent) {
    transform_->set_parent(&parent_->transform());
  }
}

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
  static_assert(std::is_base_of<GameObject, T>::value, "Unknown type");

  try {
    T *obj = new T(this, std::forward<Args>(args)...);
    components_just_added_.push_back(std::unique_ptr<GameObject>(obj));
    return obj;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    return nullptr;
  }
}

}  // namespace engine

#endif
