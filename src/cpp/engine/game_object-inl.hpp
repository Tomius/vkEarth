// Copyright (c) 2015, Tamas Csala

#ifndef ENGINE_GAME_OBJECT_INL_H_
#define ENGINE_GAME_OBJECT_INL_H_

#include <iostream>
#include "engine/game_object.hpp"

namespace engine {

template<typename Transform_t>
GameObject::GameObject(GameObject* parent, const Transform_t& transform)
    : scene_(parent ? parent->scene_ : nullptr), parent_(parent)
    , transform_(new Transform_t{transform})
    , group_(0), enabled_(true) {
  if (parent) { transform_->set_parent(parent_->transform()); }
  sorted_components_.insert(this);
}

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
  static_assert(std::is_base_of<GameObject, T>::value, "Unknown type");

  try {
    T *obj = new T(this, std::forward<Args>(args)...);
    obj->uid_ = NextUid();
    components_.push_back(std::unique_ptr<GameObject>(obj));
    components_just_enabled_.push_back(obj);

    return obj;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    return nullptr;
  }
}

inline bool GameObject::StealComponent(GameObject* go) {
  if (!go) { return false; }
  GameObject* parent = go->parent();
  if (!parent) {return false; }

  for (auto iter = parent->components_.begin();
       iter != parent->components_.end(); ++iter) {
    GameObject* comp = iter->get();
    if (comp == go) {
      components_.push_back(std::unique_ptr<GameObject>(iter->release()));
      components_just_enabled_.push_back(comp);
      parent->components_just_disabled_.push_back(comp);
      // The iter->release() leaves a nullptr in the parent->components_
      // that should be removed, as it decrases performance
      parent->RemoveComponent(nullptr);
      comp->parent_ = this;
      comp->transform_->set_parent(transform_.get());
      comp->scene_ = scene_;
      comp->uid_ = NextUid();
      return true;
    }
  }
  return false;
}

inline int GameObject::NextUid() {
  static int uid = 0;
  return uid++;
}

inline void GameObject::RemoveComponent(GameObject* component_to_remove) {
  if (component_to_remove == nullptr) { return; }
  components_just_disabled_.push_back(component_to_remove);
  remove_predicate_.components_.insert(component_to_remove);
}

}  // namespace engine

#endif
