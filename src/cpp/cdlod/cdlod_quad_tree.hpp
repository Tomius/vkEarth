#ifndef CDLOD_QUAD_TREE_H_
#define CDLOD_QUAD_TREE_H_

#include "cdlod/quad_grid_mesh.hpp"
#include "cdlod/cdlod_quad_tree_node.hpp"
#include "engine/camera.hpp"

class CdlodQuadTree {
  size_t maxNodeLevel_;
  CdlodQuadTreeNode root_;

 public:
  CdlodQuadTree(size_t kFaceSize, CubeFace face);
  void SelectNodes(const engine::Camera& cam, QuadGridMesh& mesh);
  size_t maxNodeLevel() const { return maxNodeLevel_; }
};

#endif
