// Copyright (c) 2015, Tamas Csala

#include "collision/cube2sphere.hpp"
#include "common/defines.hpp"

static glm::dvec3 Cubify(const glm::dvec3& p) {
  return {
    p.x * sqrt(1 - sqr(p.y)/2 - sqr(p.z)/2 + sqr(p.y*p.z)/3),
    p.y * sqrt(1 - sqr(p.z)/2 - sqr(p.x)/2 + sqr(p.z*p.x)/3),
    p.z * sqrt(1 - sqr(p.x)/2 - sqr(p.y)/2 + sqr(p.x*p.y)/3)
  };
}

static glm::dvec3 FaceLocalToUnitCube(const glm::dvec3& pos,
                                      CubeFace face,
                                      double kFaceSize) {
  glm::dvec3 noHeight = glm::dvec3(pos.x, 0, pos.z);
  glm::dvec3 n = (noHeight - kFaceSize/2) / (kFaceSize/2); // normalized to [-1, 1]
  switch (face) {
    case CubeFace::kPosX: return {-n.y, -n.z, -n.x}; break;
    case CubeFace::kNegX: return {+n.y, -n.z, +n.x}; break;
    case CubeFace::kPosY: return {-n.z, -n.y, +n.x}; break;
    case CubeFace::kNegY: return {+n.z, +n.y, +n.x}; break;
    case CubeFace::kPosZ: return {-n.x, -n.z, +n.y}; break;
    case CubeFace::kNegZ: return {+n.x, -n.z, -n.y}; break;
  }
}

glm::dvec3 Cube2Sphere(const glm::dvec3& pos,
                       CubeFace face,
                       double kFaceSize) {
  glm::dvec3 posOnCube = FaceLocalToUnitCube(pos, face, kFaceSize);
  return (Settings::kSphereRadius + pos.y) * Cubify(posOnCube);
}

