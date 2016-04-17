// Copyright (c) 2016, Tamas Csala

#ifndef COLLISION_SPHERIZED_AABB_HPP_
#define COLLISION_SPHERIZED_AABB_HPP_

#include "collision/sphere.hpp"
#include "collision/cube2sphere.hpp"
#include "common/settings.hpp"

class SpherizedAABB {
 public:
  SpherizedAABB() = default;
  SpherizedAABB(const glm::dvec3& mins, const glm::dvec3& maxes,
                CubeFace face, double face_size);

  bool CollidesWithSphere(const Sphere& sphere) const;
  bool CollidesWithFrustum(const Frustum& frustum) const;

 private:
  struct Interval {
    double min;
    double max;
  };

  Sphere bsphere_;
  glm::dvec3 normals_[4];
  Interval extents_[4];
  Interval radial_extent_;

  static glm::dvec3 GetNormal(glm::dvec3 vertices[], int a, int b, int c, int d);
  static bool HasIntersection(const Interval& a, const Interval& b);
  static Interval GetExtent(const glm::dvec3& normal,
                            const glm::dvec3& m_space_min,
                            const glm::dvec3& m_space_max,
                            CubeFace face, double face_size);
};


class SpherizedAABBDivided {
 public:
  SpherizedAABBDivided() = default;
  SpherizedAABBDivided(const glm::dvec3& mins, const glm::dvec3& maxes,
                       CubeFace face, double face_size);

  bool CollidesWithSphere(const Sphere& sphere) const;
  bool CollidesWithFrustum(const Frustum& frustum) const;

 private:
  static constexpr int kAabbSubdivisionRate = 2;

  SpherizedAABB main_;
  SpherizedAABB subs_[Cube(kAabbSubdivisionRate)];
};


#endif
