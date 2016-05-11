// Copyright (c) 2016, Tamas Csala

#ifndef CDLOD_QUAD_TREE_NODE_HPP_
#define CDLOD_QUAD_TREE_NODE_HPP_

#include <memory>
#include "cdlod/quad_grid_mesh.hpp"
#include "cdlod/texture_handler.hpp"
#include "collision/spherized_aabb.hpp"
#include "common/thread_pool.hpp"

class CdlodQuadTreeNode {
 public:
  CdlodQuadTreeNode(double x, double z, CubeFace face, int level,
                    CdlodQuadTreeNode* parent = nullptr);

  void Age(TextureHandler& texture_handler);
  void SelectNodes(const glm::vec3& cam_pos,
                   const Frustum& frustum,
                   QuadGridMesh& grid_mesh,
                   ThreadPool& thread_pool,
                   TextureHandler& texture_handler);

  void SelectTexture(const glm::vec3& cam_pos,
                     const Frustum& frustum,
                     ThreadPool& thread_pool,
                     StreamedTextureInfo& texinfo,
                     TextureHandler& texture_handler,
                     int recursion_level = 0);

 private:
  double x_, z_;
  CubeFace face_;
  int level_;
  SpherizedAABBDivided bbox_;
  CdlodQuadTreeNode* parent_;
  std::unique_ptr<CdlodQuadTreeNode> children_[4];
  int last_used_ = 0;

  TextureInfo texture_;

  // If a node is not used for this much time (frames), it will be unloaded.
  static const int kTimeToLiveInMemory = 1 << 5;

  double scale() const { return pow(2, level_); }
  double size() { return Settings::kNodeDimension * scale(); }
  bool CollidesWithSphere(const Sphere& sphere) const;
  void InitChild(int i);

  std::string GetHeightMapPath() const;
  std::string GetDiffuseMapPath() const;

  int ElevationTextureLevel() const;
  int DiffuseTextureLevel() const;

  bool HasElevationTexture() const;
  bool HasDiffuseTexture() const;

  void LoadTexture(bool synchronous_load);
  void Upload(TextureHandler& texture_handler);
  void CalculateMinMax();
  void RefreshMinMax();
};

#endif
