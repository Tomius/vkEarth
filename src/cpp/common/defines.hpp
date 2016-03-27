#ifndef DEFINES_HPP_
#define DEFINES_HPP_

#include <memory>

#define VK_VALIDATE 1
#define VK_VSYNC 0
#define MAX_INSTANCE_COUNT 1024*1024

static constexpr double kEpsilon = 1e-5;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>{new T(std::forward<Args>(args)...)};
}

template<typename T>
std::unique_ptr<T> make_unique() {
  return std::unique_ptr<T>{new T{}};
}

template<typename T>
constexpr T sqr(T v) {
  return v * v;
}

template<typename T>
constexpr T cube(T v) {
  return v * v * v;
}

namespace Settings {

static constexpr int kNodeDimensionExp = 4;
static constexpr int kNodeDimension = 1 << kNodeDimensionExp;

// The resolution of the heightmap
static constexpr long kFaceSize = 65536;

// The radius of the sphere made of the heightmap
static constexpr double kSphereRadius = kFaceSize / 2;

static constexpr double kMtEverestHeight = 8848 * (kSphereRadius / 6371000);
static constexpr double kHeightScale = 0.0001;
static constexpr double kMaxHeight = kHeightScale * kMtEverestHeight;

// Geometry subdivision. This practially contols zooming into the heightmap.
// If for ex. this is three, that means that a 8x8 geometry (9x9 vertices)
// corresponds to a 1x1 texture area (2x2 texels)
static constexpr long kGeomDiv = 0;

static constexpr int kLevelOffset = 0;
static constexpr double kSmallestGeometryLodDistance = 2*kNodeDimension;

}

#endif
