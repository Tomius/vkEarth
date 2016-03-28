#include <algorithm>
#include "cdlod/cdlod_quad_tree_node.hpp"
#include "collision/cube2sphere.hpp"

CdlodQuadTreeNode::CdlodQuadTreeNode(double x, double z, CubeFace face,
                                     int level, CdlodQuadTreeNode* parent)
    : x_(x), z_(z), face_(face), level_(level), parent_(parent)
    , bbox_{glm::vec3{x - size()/2, 0, z - size()/2},
            glm::vec3{x + size()/2, 0, z + size()/2},
            face, Settings::kFaceSize}
{ }

void CdlodQuadTreeNode::initChild(int i) {
  assert (0 <= i && i <= 3);

  double s4 = size()/4, x, z;
  if (i == 0) {
    x = x_-s4; z = z_+s4;
  } else if (i == 1) {
    x = x_+s4; z = z_+s4;
  } else if (i == 2) {
    x = x_-s4; z = z_-s4;
  } else if (i == 3) {
    x = x_+s4; z = z_-s4;
  }

  children_[i] = make_unique<CdlodQuadTreeNode>(x, z, face_, level_-1, this);
}

void CdlodQuadTreeNode::selectNodes(const glm::vec3& cam_pos,
                                    const Frustum& frustum,
                                    QuadGridMesh& grid_mesh) {
  last_used_ = 0;

  // textures should be loaded, even if it is outside the frustum (otherwise the
  // texture lod difference of neighbour nodes can cause geometry cracks)
  // if (!bbox_.collidesWithFrustum(frustum)) { return; }

  // If we can cover the whole area or if we are a leaf
  Sphere sphere{cam_pos, Settings::kSmallestGeometryLodDistance * scale()};
  if (!bbox_.collidesWithSphere(sphere) ||
      level_ <= Settings::kLevelOffset - Settings::kGeomDiv) {
    if (bbox_.collidesWithFrustum(frustum)) {
      grid_mesh.addToRenderList(x_, z_, level_, int(face_));
    }
  } else {
    bool cc[4]{}; // children collision

    for (int i = 0; i < 4; ++i) {
      if (!children_[i])
        initChild(i);

      cc[i] = children_[i]->collidesWithSphere(sphere);
      if (cc[i]) {
        // Ask child to render what we can't
        children_[i]->selectNodes(cam_pos, frustum, grid_mesh);
      }
    }

    if (bbox_.collidesWithFrustum(frustum)) {
      // Render what the children didn't do
      grid_mesh.addToRenderList(x_, z_, level_, int(face_),
                                !cc[0], !cc[1], !cc[2], !cc[3]);
    }
  }
}

void CdlodQuadTreeNode::age() {
  last_used_++;

  for (auto& child : children_) {
    if (child) {
      // unload child if its age would exceed the ttl
      if (child->last_used_ > kTimeToLiveInMemory) {
        child.reset();
      } else {
        child->age();
      }
    }
  }
}

bool CdlodQuadTreeNode::collidesWithSphere(const Sphere& sphere) const {
  return bbox_.collidesWithSphere(sphere);
}
