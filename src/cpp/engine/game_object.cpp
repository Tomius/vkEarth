// Copyright (c) 2015, Tamas Csala

#include "engine/scene.hpp"
#include "engine/game_object.hpp"
#include "engine/game_engine.hpp"

#define _TRY_(YourCode) \
  try { \
    YourCode; \
  } catch (const std::exception& ex) { \
    std::cerr << "Exception: " << ex.what() << std::endl; \
  } catch (...) {}

namespace engine {

GameObject::~GameObject() {
  // The childrens destructor have to run before this one's,
  // as those functions might try to access this object via the parent_ ptr
  for (auto& comp_ptr : components_) {
    comp_ptr.reset();
  }
  // this shouldn't be neccessary, but just in case...
  if (parent_) {
    parent_->RemoveComponent(this);
  }
}

GameObject* GameObject::AddComponent(std::unique_ptr<GameObject>&& component) {
  try {
    GameObject *obj = component.get();
    components_.push_back(std::move(component));
    components_just_enabled_.push_back(obj);
    obj->parent_ = this;
    obj->uid_ = NextUid();
    obj->transform_->set_parent(transform_.get());
    obj->scene_ = scene_;

    return obj;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    return nullptr;
  }
}

void GameObject::set_parent(GameObject* parent) {
  parent_ = parent;
  if (parent) { transform_->set_parent(parent_->transform()); }
}

void GameObject::set_enabled(bool value) {
  enabled_ = value;
  if (value) {
    components_just_enabled_.push_back(this);
    if (parent_) {
      parent_->components_just_enabled_.push_back(this);
    }
  } else {
    components_just_disabled_.push_back(this);
    if (parent_) {
      parent_->components_just_disabled_.push_back(this);
    }
  }
}

void GameObject::set_group(int value) {
  group_ = value;
  components_just_enabled_.push_back(this);
  components_just_disabled_.push_back(this);
  if (parent_) {
    parent_->components_just_enabled_.push_back(this);
    parent_->components_just_disabled_.push_back(this);
  }
}

void GameObject::RenderAll() {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(Render());
    } else {
      component->RenderAll();
    }
  }
}

void GameObject::Render2DAll() {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(Render2D());
    } else {
      component->Render2DAll();
    }
  }
}

void GameObject::ScreenResizedCleanAll() {
  for (auto& component : sorted_components_) {
    if (component != this) {
      component->ScreenResizedCleanAll();
    }
  }
  _TRY_(ScreenResizedClean());
}

void GameObject::ScreenResizedAll(size_t width, size_t height) {
  _TRY_(ScreenResized(width, height));
  for (auto& component : sorted_components_) {
    if (component != this) {
      component->ScreenResizedAll(width, height);
    }
  }
}

void GameObject::UpdateAll() {
  InternalUpdate();
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(Update());
    } else {
      component->UpdateAll();
    }
  }
}

void GameObject::KeyActionAll(int key, int scancode, int action, int mods) {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(KeyAction(key, scancode, action, mods));
    } else {
      component->KeyActionAll(key, scancode, action, mods);
    }
  }
}

void GameObject::CharTypedAll(unsigned codepoint) {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(CharTyped(codepoint));
    } else {
      component->CharTypedAll(codepoint);
    }
  }
}

void GameObject::MouseScrolledAll(double xoffset, double yoffset) {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(MouseScrolled(xoffset, yoffset));
    } else {
      component->MouseScrolledAll(xoffset, yoffset);
    }
  }
}

void GameObject::MouseButtonPressedAll(int button, int action, int mods) {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(MouseButtonPressed(button, action, mods));
    } else {
      component->MouseButtonPressedAll(button, action, mods);
    }
  }
}

void GameObject::MouseMovedAll(double xpos, double ypos) {
  for (auto& component : sorted_components_) {
    if (component == this) {
      _TRY_(MouseMoved(xpos, ypos));
    } else {
      component->MouseMovedAll(xpos, ypos);
    }
  }
}

void GameObject::InternalUpdate() {
  RemoveComponents();
  UpdateSortedComponents();
}

void GameObject::UpdateSortedComponents() {
  RemoveComponents();
  for (const auto& element : components_just_disabled_) {
    sorted_components_.erase(element);
  }
  components_just_disabled_.clear();
  sorted_components_.insert(components_just_enabled_.begin(),
                            components_just_enabled_.end());
  // make sure all the componenets just enabled are aware of the screen's size
  int width, height;
  glfwGetWindowSize(scene()->window(), &width, &height);
  for (const auto& component : components_just_enabled_) {
    component->ScreenResizedAll(width, height);
  }
  components_just_enabled_.clear();
}

void GameObject::RemoveComponents() {
  if (!remove_predicate_.components_.empty()) {
    components_.erase(std::remove_if(components_.begin(), components_.end(),
      remove_predicate_), components_.end());
    remove_predicate_.components_.clear();
  }
}

bool GameObject::CompareGameObjects::operator()(GameObject* x,
                                                GameObject* y) const {
  assert(x && y);
  if (x->group() == y->group()) {
    // if x and y are in the same group and they are in a parent child
    // relation, then the parent should be handled first
    if (y->parent() == x) {
      return true;
    } else if (x->parent() == y) {
      return false;
    } else {
      // if x and y aren't "relatives", then the
      // order of adding them should count
      return x->uid_ < y->uid_;
    }
  } else {
    return x->group() < y->group();
  }
}

}  // namespace engine
