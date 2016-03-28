// Copyright (c) 2015, Tamas Csala

#ifndef ENGINE_CDLOD_QUAD_TREE_NODE_H_
#define ENGINE_CDLOD_QUAD_TREE_NODE_H_

#include <memory>
#include "cdlod/quad_grid_mesh.hpp"
#include "collision/spherized_aabb.hpp"
#include "collision/bounding_box.hpp"

class CdlodQuadTreeNode {
 public:
  CdlodQuadTreeNode(double x, double z, CubeFace face, int level,
                    CdlodQuadTreeNode* parent = nullptr);

  void age();

  void selectNodes(const glm::vec3& cam_pos,
                   const Frustum& frustum,
                   QuadGridMesh& grid_mesh);

 private:
  double x_, z_;
  CubeFace face_;
  int level_;
  SpherizedAABBDivided bbox_;
  // BoundingBox bbox_;
  CdlodQuadTreeNode* parent_;
  std::unique_ptr<CdlodQuadTreeNode> children_[4];
  int last_used_ = 0;

  // If a node is not used for this much time (frames), it will be unloaded.
  static const int kTimeToLiveInMemory = 1 << 6;

  // --- functions ---

  double scale() const { return pow(2, level_); }
  double size() { return Settings::kNodeDimension * scale(); }

  bool collidesWithSphere(const Sphere& sphere) const;

  void initChild(int i);
};

#endif
