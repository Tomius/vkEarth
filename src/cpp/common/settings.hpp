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
static constexpr int kMaxTextureCount = 2*1024; // TODO
static constexpr int kMaxTextureCountTemp = 6; // TODO

static constexpr int kNodeDimensionExp = 5;
static constexpr int kNodeDimension = 1 << kNodeDimensionExp;

static constexpr int kTextureDimensionExp = 8;
static constexpr int kTextureDimension = 1 << kTextureDimensionExp;

static constexpr int kTexDimOffset = kTextureDimensionExp - kNodeDimensionExp;

static constexpr int kElevationTexBorderSize = 3;
static constexpr int kElevationTexSizeWithBorders =
    kTextureDimension + 2*kElevationTexBorderSize;
static constexpr int kDiffuseTexBorderSize = 2;
static constexpr int kDiffuseTexSizeWithBorders =
    kTextureDimension + 2*kDiffuseTexBorderSize;

// The resolution of the heightmap
static constexpr long kFaceSize = 65536;

// The radius of the sphere made of the heightmap
static constexpr double kSphereRadius = kFaceSize / 2;

static constexpr double kMtEverestHeight = 8848 * (kSphereRadius / 6371000);
static constexpr double kScaleOfRealisticHeight = 5.0;
static constexpr double kMaxHeight = kScaleOfRealisticHeight * kMtEverestHeight;

// Geometry subdivision. This practially contols zooming into the heightmap.
// If for ex. this is three, that means that a 8x8 geometry (9x9 vertices)
// corresponds to a 1x1 texture area (2x2 texels)
static constexpr long kGeomDiv = 2;

static constexpr int kLevelOffset = 0;
static constexpr int kDiffuseToElevationLevelOffset = 1;
static constexpr int kNormalToGeometryLevelOffset = 2;

static constexpr double kSmallestGeometryLodDistance = 2*kNodeDimension;
static constexpr double kSmallestTextureLodDistance =
    kSmallestGeometryLodDistance * (1 << kNormalToGeometryLevelOffset);

static constexpr bool kWireframe = false;

static_assert(3 <= kNodeDimensionExp && kNodeDimensionExp <= 8, "");
static_assert(kNodeDimension <= kSmallestGeometryLodDistance, "");
static_assert(0 <= kNormalToGeometryLevelOffset, "");
static_assert(kNodeDimensionExp + kNormalToGeometryLevelOffset <= kTextureDimensionExp, "");
static_assert(kTextureDimension <= kSmallestTextureLodDistance, "");
static_assert(kSmallestGeometryLodDistance <= kSmallestTextureLodDistance, "");
static_assert(kGeomDiv < kNodeDimensionExp, "");

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
