// Copyright (c) 2016, Tamas Csala

#ifndef COLLISION_FRUSTUM_H_
#define COLLISION_FRUSTUM_H_

#include "plane.hpp"

struct Frustum {
  Plane planes[6]; // left, right, top, down, near, far
};

#endif
