// Copyright (c) 2015, Tamas Csala

#ifndef ENGINE_COLLISION_PLANE_H_
#define ENGINE_COLLISION_PLANE_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct Plane {
  glm::dvec3 normal;
  double dist;
  Plane() = default;
  Plane(double nx, double ny, double nz, double dist)
      : normal(nx, ny, nz), dist(dist) { normalize(); }
  Plane(const glm::dvec3& normal, double dist)
      : normal(normal), dist(dist) { normalize(); }

  void normalize() {
    double l = glm::length(normal);
    normal /= l;
    dist /= l;
  }
};

#endif
