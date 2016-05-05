// Copyright (c) 2016, Tamas Csala

#ifndef CDLOD_QUAD_TREE_HPP_
#define CDLOD_QUAD_TREE_HPP_

#include "cdlod/quad_grid_mesh.hpp"
#include "cdlod/cdlod_quad_tree_node.hpp"
#include "engine/camera.hpp"
#include "common/thread_pool.hpp"

class CdlodQuadTree {
  size_t max_node_level_;
  CdlodQuadTreeNode root_;

 public:
  CdlodQuadTree(size_t kFaceSize, CubeFace face);
  void SelectNodes(const engine::Camera& cam,
                   QuadGridMesh& mesh,
                   ThreadPool& thread_pool,
                   TextureHandler& texture_handler);
  size_t max_node_level() const { return max_node_level_; }
};

#endif
