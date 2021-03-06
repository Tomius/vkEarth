// Copyright (c) 2016, Tamas Csala

#ifndef COLLISION_PLANE_H_
#define COLLISION_PLANE_H_

#include <cassert>
#include "common/glm.hpp"
#include "common/settings.hpp"

struct Plane {
  glm::dvec3 normal;
  double dist = 0.0;

  Plane() = default;
  Plane(double nx, double ny, double nz, double dist)
      : normal(nx, ny, nz), dist(dist) { Normalize(); }
  Plane(const glm::dvec3& normal, double dist)
      : normal(normal), dist(dist) { Normalize(); }

  void Normalize() {
    double l = glm::length(normal);
    assert(l > Settings::kEpsilon);
    normal /= l;
    dist /= l;
  }
};

#endif
