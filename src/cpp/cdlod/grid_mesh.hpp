#ifndef CDLOD_GRID_MESH_H_
#define CDLOD_GRID_MESH_H_

#include <vector>
#include <cstdint>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// A two-dimensional vector of GLshort values
struct svec2 {
  uint16_t x, y;
  svec2() : x(0), y(0) {}
  svec2(uint16_t a, uint16_t b) : x(a), y(b) {}
  svec2 operator+(const svec2 rhs) { return svec2(x + rhs.x, y + rhs.y); }
  friend svec2 operator*(uint16_t lhs, const svec2 rhs) {
    return svec2(lhs * rhs.x, lhs * rhs.y);
  }
};

// Renders a regular grid mesh, that is of (dimension+1) x (dimension+1) in size
// so a GridMesh(16) will go from (-8, -8) to (8, 8). It is designed to render
// a lots of this at the same time, with instanced rendering.
//
// For performance reasons, GridMesh's maximum size is 255*255 (so that it can
// use unsigned shorts instead of ints or floats), but for CDLOD, you need
// pow2 sizes, so there 128*128 is the max
class GridMesh {
  uint16_t indexOf(int x, int y);

 public:
  int index_count_, dimension_;
  std::vector<svec2> positions_;
  std::vector<uint16_t> indices_;
  std::vector<glm::vec4> renderData_; // xy: offset, z: level, w: face

  GridMesh(uint8_t dimension);

  void addToRenderList(const glm::vec4& renderData);
  void clearRenderList();

  void render();

  int dimension() const {return dimension_;}
  size_t node_count() const { return renderData_.size(); }
};

#endif // CDLOD_GRID_MESH_H_
