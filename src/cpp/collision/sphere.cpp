// Copyright (c) 2016, Tamas Csala

#include "collision/sphere.hpp"

bool Sphere::CollidesWithSphere(const Sphere& sphere) const {
  double dist = glm::length(center_ - sphere.center_);
  return dist < radius_ + sphere.radius_;
}

// http://www.flipcode.com/archives/Frustum_Culling.shtml
bool Sphere::CollidesWithFrustum(const Frustum& frustum) const {
  // calculate our distances to each of the planes
  for (int i = 0; i < 6; ++i) {
    const Plane& plane = frustum.planes[i];

    // find the distance to this plane
    double dist = glm::dot(plane.normal, center_) + plane.dist;

    // if this distance is < -sphere.radius, we are outside
    if (dist < -radius_)
      return false;

    // else if the distance is between +- radius, then we intersect
    if (std::abs(dist) < radius_)
      return true;
  }

  // otherwise we are fully in view
  return true;
}
