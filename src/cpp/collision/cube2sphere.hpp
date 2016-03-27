#ifndef CUBE2SPHERE_H_
#define CUBE2SPHERE_H_

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

enum class CubeFace {
  kPosX, kNegX, kPosY, kNegY, kPosZ, kNegZ
};

glm::dvec3 Cube2Sphere(const glm::dvec3& pos, CubeFace face, double kFaceSize);


#endif
