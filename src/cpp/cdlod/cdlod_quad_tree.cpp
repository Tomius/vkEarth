// Copyright (c) 2016, Tamas Csala

#include "cdlod/cdlod_quad_tree.hpp"

CdlodQuadTree::CdlodQuadTree(size_t kFaceSize, CubeFace face)
  : max_node_level_(log2(kFaceSize) - Settings::kNodeDimensionExp)
  , root_(kFaceSize/2, kFaceSize/2, face, max_node_level_) {}

void CdlodQuadTree::SelectNodes(const engine::Camera& cam, QuadGridMesh& mesh,
                                ThreadPool& thread_pool) {
  root_.SelectNodes(cam.transform().pos(), cam.frustum(), mesh, thread_pool);
  root_.Age();
}

