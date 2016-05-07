// Copyright (c) 2016, Tamas Csala

#include "engine/camera.hpp"
#include "engine/scene.hpp"

namespace engine {

Camera::Camera(GameObject* parent, double fovy, double z_near, double z_far)
    : GameObject(parent, CameraTransform{}), fovy_(fovy), z_near_(z_near)
    , z_far_(z_far), width_(0), height_(0) {
  assert(fovy_ < M_PI);
}

void Camera::ScreenResized(size_t width, size_t height) {
  width_ = width;
  height_ = height;
}

void Camera::UpdateCache() {
  UpdateCameraMatrix();
  UpdateProjectionMatrix();
  UpdateFrustum();
}

void Camera::UpdateCameraMatrix() {
  const Transform& t = transform();
  cam_mat_ = glm::lookAt(t.pos(), t.pos()+t.forward(), t.up());
}

void Camera::UpdateProjectionMatrix() {
  proj_mat_ = glm::perspectiveFov<double>(fovy_, width_, height_, z_near_, z_far_);
}

void Camera::UpdateFrustum() {
  glm::dmat4 m = proj_mat_ * cam_mat_;

  // REMEMBER: m[i][j] is j-th row, i-th column (glm is column major)

  frustum_ = Frustum{{
    // left
   {m[0][3] + m[0][0],
    m[1][3] + m[1][0],
    m[2][3] + m[2][0],
    m[3][3] + m[3][0]},

    // right
   {m[0][3] - m[0][0],
    m[1][3] - m[1][0],
    m[2][3] - m[2][0],
    m[3][3] - m[3][0]},

    // top
   {m[0][3] - m[0][1],
    m[1][3] - m[1][1],
    m[2][3] - m[2][1],
    m[3][3] - m[3][1]},

    // bottom
   {m[0][3] + m[0][1],
    m[1][3] + m[1][1],
    m[2][3] + m[2][1],
    m[3][3] + m[3][1]},

    // near
   {m[0][2],
    m[1][2],
    m[2][2],
    m[3][2]},

    // far
   {m[0][3] - m[0][2],
    m[1][3] - m[1][2],
    m[2][3] - m[2][2],
    m[3][3] - m[3][2]}
  }}; // ctor normalizes the planes
}

FreeFlyCamera::FreeFlyCamera(GameObject* parent, double fov, double z_near,
                             double z_far, const glm::dvec3& pos,
                             const glm::dvec3& target /*= glm::dvec3()*/,
                             double speed_per_sec /*= 5.0f*/,
                             double mouse_sensitivity /*= 5.0f*/)
    : Camera(parent, fov, z_near, z_far)
    , first_call_(true)
    , speed_per_sec_(speed_per_sec)
    , mouse_sensitivity_(mouse_sensitivity)
    , cos_max_pitch_angle_(0.98f) {
  transform().set_pos(pos);
  transform().set_forward(target - pos);
}

void FreeFlyCamera::Update() {
  glm::dvec2 cursor_pos;
  GLFWwindow* window = scene_->window();
  glfwGetCursorPos(window, &cursor_pos.x, &cursor_pos.y);
  static glm::dvec2 prev_cursor_pos;
  glm::dvec2 diff = cursor_pos - prev_cursor_pos;
  prev_cursor_pos = cursor_pos;

  // We get invalid diff values at the startup
  if (first_call_) {
    diff = glm::dvec2(0, 0);
    first_call_ = false;
  }

  const double dt = scene_->camera_time().dt();

  // Mouse movement - update the coordinate system
  if (diff.x || diff.y) {
    double dx(diff.x * mouse_sensitivity_ / 10000);
    double dy(-diff.y * mouse_sensitivity_ / 10000);

    // If we are looking up / down, we don't want to be able
    // to rotate to the other side
    double dot_up_fwd = glm::dot(transform().up(), transform().forward());
    if (dot_up_fwd > cos_max_pitch_angle_ && dy > 0) {
      dy = 0;
    }
    if (dot_up_fwd < -cos_max_pitch_angle_ && dy < 0) {
      dy = 0;
    }

    transform().set_forward(transform().forward() +
                             transform().right()*dx +
                             transform().up()*dy);
  }

  // Update the position
  double ds = dt * speed_per_sec_;
  glm::dvec3 local_pos = transform().local_pos();
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    local_pos += transform().forward() * ds;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    local_pos -= transform().forward() * ds;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    local_pos += transform().right() * ds;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    local_pos -= transform().right() * ds;
  }
  transform().set_local_pos(local_pos);

  Camera::UpdateCache();
}

ThirdPersonalCamera::ThirdPersonalCamera(GameObject* parent,
                                         double fov,
                                         double z_near,
                                         double z_far,
                                         const glm::dvec3& position,
                                         double mouse_sensitivity /*= 1.0*/,
                                         double mouse_scroll_sensitivity /*= 1.0*/,
                                         double min_dist_mod /*= 0.25*/,
                                         double max_dist_mod /*= 4.00*/,
                                         double base_distance /*= 0.0*/,
                                         double dist_offset /*= 0.0*/)
    : Camera(parent, fov, z_near, z_far)
    , target_(parent->transform())
    , first_call_(true)
    , initial_distance_(glm::length(target_.pos() - position) - dist_offset)
    , base_distance_(base_distance == 0.0 ? initial_distance_ : base_distance)
    , cos_max_pitch_angle_(0.999)
    , mouse_sensitivity_(mouse_sensitivity)
    , mouse_scroll_sensitivity_(mouse_scroll_sensitivity)
    , min_dist_mod_(min_dist_mod)
    , max_dist_mod_(max_dist_mod)
    , dist_offset_(dist_offset)
    , curr_dist_mod_(initial_distance_ / base_distance_)
    , dest_dist_mod_(curr_dist_mod_){
  transform().set_pos(position);
  transform().set_forward(target_.pos() - position);
}

void ThirdPersonalCamera::Update() {
  static glm::dvec2 prev_cursor_pos;
  glm::dvec2 cursor_pos;
  GLFWwindow* window = scene_->window();
  glfwGetCursorPos(window, &cursor_pos.x, &cursor_pos.y);
  glm::dvec2 diff = cursor_pos - prev_cursor_pos;
  prev_cursor_pos = cursor_pos;

  // We get invalid diff values at the startup
  if (first_call_ && (diff.x != 0 || diff.y != 0)) {
    diff = glm::dvec2(0, 0);
    first_call_ = false;
  }

  const double dt = scene_->camera_time().dt();

  // Mouse movement - update the coordinate system
  if (diff.x || diff.y) {
    double mouse_sensitivity = mouse_sensitivity_ * curr_dist_mod_ / 10000;
    double dx(diff.x * mouse_sensitivity);
    double dy(-diff.y * mouse_sensitivity);

    // If we are looking up / down, we don't want to be able
    // to rotate to the other side
    double dot_up_fwd = glm::dot(transform().up(), transform().forward());
    if (dot_up_fwd > cos_max_pitch_angle_ && dy > 0) {
      dy = 0;
    }
    if (dot_up_fwd < -cos_max_pitch_angle_ && dy < 0) {
      dy = 0;
    }

    transform().set_forward(transform().forward() +
                             transform().right()*dx +
                             transform().up()*dy);
  }

  double dist_diff_mod = dest_dist_mod_ - curr_dist_mod_;
  if (fabs(dist_diff_mod) > dt * mouse_scroll_sensitivity_) {
    curr_dist_mod_ *= dist_diff_mod > 0 ?
      (1 + dt * mouse_scroll_sensitivity_) :
      (1 - dt * mouse_scroll_sensitivity_);
  }

  // Update the position
  glm::dvec3 tpos(target_.pos()), fwd(transform().forward());
  fwd = transform().forward();
  double dist = curr_dist_mod_*base_distance_ + dist_offset_;
  glm::dvec3 pos = tpos - fwd*dist;
  transform().set_pos(pos);

  Camera::UpdateCache();
}

void ThirdPersonalCamera::MouseScrolled(double, double yoffset) {
  dest_dist_mod_ *= 1 + (-yoffset) * 0.1 * mouse_scroll_sensitivity_;
  if (dest_dist_mod_ < min_dist_mod_) {
    dest_dist_mod_ = min_dist_mod_;
  } else if (dest_dist_mod_ > max_dist_mod_) {
    dest_dist_mod_ = max_dist_mod_;
  }
}

}  // namespace engine
