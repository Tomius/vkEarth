// Copyright (c) 2016, Tamas Csala

#ifndef CDLOD_QUAD_TREE_NODE_H_
#define CDLOD_QUAD_TREE_NODE_H_

#include <memory>
#include "cdlod/quad_grid_mesh.hpp"
#include "collision/spherized_aabb.hpp"

class CdlodQuadTreeNode {
 public:
  CdlodQuadTreeNode(double x, double z, CubeFace face, int level,
                    CdlodQuadTreeNode* parent = nullptr);

  void Age();
  void SelectNodes(const glm::vec3& cam_pos,
                   const Frustum& frustum,
                   QuadGridMesh& grid_mesh);

 private:
  double x_, z_;
  CubeFace face_;
  int level_;
  SpherizedAABBDivided bbox_;
  std::unique_ptr<CdlodQuadTreeNode> children_[4];
  int last_used_ = 0;

  // If a node is not used for this much time (frames), it will be unloaded.
  static const int kTimeToLiveInMemory = 1 << 6;

  double scale() const { return pow(2, level_); }
  double size() { return Settings::kNodeDimension * scale(); }
  bool CollidesWithSphere(const Sphere& sphere) const;
  void InitChild(int i);
};

#endif
