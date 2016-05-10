// Copyright (c) 2016, Tamas Csala

#include "engine/scene.hpp"
#include "engine/game_object.hpp"
#include "engine/game_engine.hpp"

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
    components_just_added_.push_back(std::move(component));
    obj->parent_ = this;
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
  if (parent) { transform_->set_parent(&parent_->transform()); }
}

void GameObject::RenderAll() {
  if (!enabled_) { return; }

  Render();
  for (auto& component : components_) {
    component->RenderAll();
  }
}

void GameObject::Render2DAll() {
  if (!enabled_) { return; }

  Render2D();
  for (auto& component : components_) {
    component->Render2DAll();
  }
}

void GameObject::ScreenResizedCleanAll() {
  if (!enabled_) { return; }

  for (auto& component : components_) {
    component->ScreenResizedCleanAll();
  }
  ScreenResizedClean();
}

void GameObject::ScreenResizedAll(size_t width, size_t height) {
  if (!enabled_) { return; }

  ScreenResized(width, height);
  for (auto& component : components_) {
    component->ScreenResizedAll(width, height);
  }
}

void GameObject::UpdateAll() {
  if (!enabled_) { return; }

  InternalUpdate();
  Update();
  for (auto& component : components_) {
    component->UpdateAll();
  }
}

void GameObject::KeyActionAll(int key, int scancode, int action, int mods) {
  if (!enabled_) { return; }

  KeyAction(key, scancode, action, mods);
  for (auto& component : components_) {
    component->KeyActionAll(key, scancode, action, mods);
  }
}

void GameObject::CharTypedAll(unsigned codepoint) {
  if (!enabled_) { return; }

  CharTyped(codepoint);
  for (auto& component : components_) {
    component->CharTypedAll(codepoint);
  }
}

void GameObject::MouseScrolledAll(double xoffset, double yoffset) {
  if (!enabled_) { return; }

  MouseScrolled(xoffset, yoffset);
  for (auto& component : components_) {
    component->MouseScrolledAll(xoffset, yoffset);
  }
}

void GameObject::MouseButtonPressedAll(int button, int action, int mods) {
  if (!enabled_) { return; }

  MouseButtonPressed(button, action, mods);
  for (auto& component : components_) {
    component->MouseButtonPressedAll(button, action, mods);
  }
}

void GameObject::MouseMovedAll(double xpos, double ypos) {
  if (!enabled_) { return; }

  MouseMoved(xpos, ypos);
  for (auto& component : components_) {
    component->MouseMovedAll(xpos, ypos);
  }
}

void GameObject::InternalUpdate() {
  RemoveComponents();
  AddNewComponents();
}

void GameObject::AddNewComponents() {
  if (!components_just_added_.empty()) {
    // make sure all the componenets just enabled are aware of the screen's size
    int width, height;
    glfwGetWindowSize(scene()->window(), &width, &height);
    for (const auto& component : components_just_added_) {
      component->ScreenResizedAll(width, height);
    }

    // move them to their new place
    for (auto& component : components_just_added_) {
      components_.push_back(std::move(component));
    }

    components_just_added_.clear();
  }
}

void GameObject::RemoveComponents() {
  if (!components_to_remove_.empty()) {
    components_.erase(std::remove_if(components_.begin(), components_.end(),
      [&](const std::unique_ptr<GameObject>& go_ptr){
        return components_to_remove_.find(go_ptr.get()) != components_to_remove_.end();
      }), components_.end());
    components_to_remove_.clear();
  }
}

bool GameObject::StealComponent(GameObject* go) {
  if (!go) { return false; }
  GameObject* parent = go->parent();
  if (!parent) { return false; }

  for (auto& component : parent->components_) {
    if (component.get() == go) {
      components_just_added_.push_back(std::move(component));
      // The move leaves a nullptr in the parent->components_
      // that should be removed, as it decrases performance
      parent->RemoveComponent(nullptr);
      component->parent_ = this;
      component->transform_->set_parent(transform_.get());
      component->scene_ = scene_;
      return true;
    }
  }
  return false;
}

void GameObject::RemoveComponent(GameObject* component_to_remove) {
  if (component_to_remove) {
    components_to_remove_.insert(component_to_remove);
  }
}

}  // namespace engine
