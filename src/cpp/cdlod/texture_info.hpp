// Copyright (c) 2016, Tamas Csala

#ifndef CDLOD_TEXTURE_INFO_HPP_
#define CDLOD_TEXTURE_INFO_HPP_

#include <mutex>
#include <limits>
#include "common/glm.hpp"
#include "common/settings.hpp"

struct RGBAPixel {
  unsigned char r, g, b, a;
};

struct TextureBaseInfo {
  unsigned id = -1;
  glm::vec2 position; // top-left
  float size = 0;
};
static_assert(sizeof(TextureBaseInfo) == sizeof(unsigned) + sizeof(glm::vec3), "");

class CdlodQuadTreeNode;

struct TextureInfo {
  TextureBaseInfo elevation, diffuse;

  uint16_t min = std::numeric_limits<uint16_t>::max();
  uint16_t max = std::numeric_limits<uint16_t>::min();
  double min_h = 0;
  double max_h = Settings::kMaxHeight;

  CdlodQuadTreeNode* min_max_src = nullptr;

  std::vector<uint16_t> elevation_data;
  std::vector<RGBAPixel> diffuse_data;
  bool is_loaded_to_gpu = false;

  std::mutex load_mutex;
  bool is_loaded_to_memory = false;
};

struct StreamedTextureInfo {
  TextureBaseInfo geometry_current;
  TextureBaseInfo geometry_next;
  TextureBaseInfo normal_current;
  TextureBaseInfo normal_next;
  TextureBaseInfo diffuse_current;
  TextureBaseInfo diffuse_next;
};

struct PerInstanceAttributes {
  glm::vec4 render_data; // xy: offset, z: level, w: face
  StreamedTextureInfo texture_info;
};

#endif
