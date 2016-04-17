// Copyright (c) 2016, Tamas Csala

#ifndef COLLISION_CUBE2SPHERE_HPP_
#define COLLISION_CUBE2SPHERE_HPP_

#include "common/glm.hpp"

enum class CubeFace {
  kPosX, kNegX, kPosY, kNegY, kPosZ, kNegZ
};

glm::dvec3 Cube2Sphere(const glm::dvec3& pos, CubeFace face, double kFaceSize);


#endif
