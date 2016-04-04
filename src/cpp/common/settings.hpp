// Copyright (c) 2016, Tamas Csala

#ifndef COMMON_SETTINGS_HPP_
#define COMMON_SETTINGS_HPP_

#include <memory>

#ifndef VK_VALIDATE
  #ifdef VK_DEBUG
    #define VK_VALIDATE 1
  #else
    #define VK_VALIDATE 0
  #endif
#endif
#define VK_VSYNC 0

namespace Settings {

static constexpr double kEpsilon = 1e-5;

static constexpr int kMaxInstanceCount = 32*1024; // TODO

static constexpr int kNodeDimensionExp = 4;
static constexpr int kNodeDimension = 1 << kNodeDimensionExp;

// The resolution of the heightmap
static constexpr long kFaceSize = 65536;

// The radius of the sphere made of the heightmap
static constexpr double kSphereRadius = kFaceSize / 2;

static constexpr double kMtEverestHeight = 8848 * (kSphereRadius / 6371000);
static constexpr double kScaleOfRealisticHeight = 128;
static constexpr double kMaxHeight = kScaleOfRealisticHeight * kMtEverestHeight;

// Geometry subdivision. This practially contols zooming into the heightmap.
// If for ex. this is three, that means that a 8x8 geometry (9x9 vertices)
// corresponds to a 1x1 texture area (2x2 texels)
static constexpr long kGeomDiv = 0;

static constexpr int kLevelOffset = 0;
static constexpr double kSmallestGeometryLodDistance = 2*kNodeDimension;

}

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>{new T(std::forward<Args>(args)...)};
}

template<typename T>
std::unique_ptr<T> make_unique() {
  return std::unique_ptr<T>{new T{}};
}

template<typename T>
constexpr T Sqr(T v) {
  return v * v;
}

template<typename T>
constexpr T Cube(T v) {
  return v * v * v;
}

#endif
