// Copyright (c) 2015, Tamas Csala

#ifndef ENGINE_GAME_OBJECT_H_
#define ENGINE_GAME_OBJECT_H_

#include <set>
#include <memory>
#include <vector>
#include <iostream>
#include <algorithm>

#include "engine/transform.hpp"

namespace engine {

class Scene;

class GameObject {
 public:
  template<typename Transform_t = Transform>
  explicit GameObject(GameObject* parent,
                      const Transform_t& initial_transform = Transform_t{});
  virtual ~GameObject();

  template<typename T, typename... Args>
  T* AddComponent(Args&&... contructor_args);
  GameObject* AddComponent(std::unique_ptr<GameObject>&& component);

  // Detaches a componenent from its parent, and adopts it.
  // Returns true on success.
  bool StealComponent(GameObject* component_to_steal);

  void RemoveComponent(GameObject* component_to_remove);

  Transform* transform() { return transform_.get(); }
  const Transform* transform() const { return transform_.get(); }

  GameObject* parent() { return parent_; }
  const GameObject* parent() const { return parent_; }
  void set_parent(GameObject* parent);

  Scene* scene() { return scene_; }
  const Scene* scene() const { return scene_; }
  void set_scene(Scene* scene) { scene_ = scene; }

  bool enabled() const { return enabled_; }
  void set_enabled(bool value);

  int group() const { return group_; }
  void set_group(int value);

  virtual void Render() {}
  virtual void Render2D() {}
  virtual void Update() {}
  virtual void ScreenResizedClean() {}
  virtual void ScreenResized(size_t width, size_t height) {}
  virtual void KeyAction(int key, int scancode, int action, int mods) {}
  virtual void CharTyped(unsigned codepoint) {}
  virtual void MouseScrolled(double xoffset, double yoffset) {}
  virtual void MouseButtonPressed(int button, int action, int mods) {}
  virtual void MouseMoved(double xpos, double ypos) {}

  virtual void RenderAll();
  virtual void Render2DAll();
  virtual void UpdateAll();
  virtual void ScreenResizedCleanAll();
  virtual void ScreenResizedAll(size_t width, size_t height);
  virtual void KeyActionAll(int key, int scancode, int action, int mods);
  virtual void CharTypedAll(unsigned codepoint);
  virtual void MouseScrolledAll(double xoffset, double yoffset);
  virtual void MouseButtonPressedAll(int button, int action, int mods);
  virtual void MouseMovedAll(double xpos, double ypos);

 protected:
  Scene* scene_;
  GameObject* parent_;
  std::unique_ptr<Transform> transform_;
  std::vector<std::unique_ptr<GameObject>> components_;
  std::vector<GameObject*> components_just_enabled_, components_just_disabled_;

  struct CompareGameObjects {
    bool operator() (GameObject* x, GameObject* y) const;
  };

  std::set<GameObject*, CompareGameObjects> sorted_components_;
  int uid_, group_;
  bool enabled_;

  void InternalUpdate();

 private:
  static int NextUid();

  struct ComponentRemoveHelper {
    std::set<GameObject*> components_;
    bool operator()(const std::unique_ptr<GameObject>& go_ptr) {
      return components_.find(go_ptr.get()) != components_.end();
    }
  } remove_predicate_;

  void UpdateSortedComponents();
  void RemoveComponents();
};

}  // namespace engine

#include "engine/game_object-inl.hpp"

#endif
