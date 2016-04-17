// Copyright (c) 2016, Tamas Csala

#ifndef CDLOD_QUAD_GRID_MESH_H_
#define CDLOD_QUAD_GRID_MESH_H_

#include "common/settings.hpp"
#include "cdlod/grid_mesh.hpp"

// Makes up four, separately renderable GridMeshes.
class QuadGridMesh {
 public:
  GridMesh mesh_;

  // Specify the size of the 4 subquads together, not the size of one subquad
  // It should be between 2 and 256, and should be a power of 2
  QuadGridMesh(int dimension = Settings::kNodeDimension);

  // Adds a subquad to the render list. tl = top left, br = bottom right
  void AddToRenderList(float offset_x, float offset_y, int level, int face,
                       const StreamedTextureInfo& texture_info,
                       bool tl, bool tr, bool bl, bool br);
  // Adds all four subquads
  void AddToRenderList(float offset_x, float offset_y, int level, int face,
                       const StreamedTextureInfo& texture_info);
  void ClearRenderList();
  // void render();
  size_t node_count() const;
};

#endif
