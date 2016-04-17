// Copyright (c) 2016, Tamas Csala

#ifndef COLLISION_SPHERE_HPP_
#define COLLISION_SPHERE_HPP_

#include "common/glm.hpp"
#include "collision/frustum.hpp"

class Sphere {
 private:
  glm::dvec3 center_;
  double radius_ = 0.0;

 public:
  Sphere() = default;
  Sphere(glm::dvec3 const& center, double radius)
      : center_(center), radius_(radius) {}

  glm::dvec3 center() const { return center_; }
  double radius() const { return radius_; }

  virtual bool CollidesWithSphere(const Sphere& sphere) const;
  virtual bool CollidesWithFrustum(const Frustum& frustum) const;
};


#endif
