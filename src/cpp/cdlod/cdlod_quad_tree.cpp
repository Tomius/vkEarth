#include "cdlod/cdlod_quad_tree.hpp"

CdlodQuadTree::CdlodQuadTree(size_t kFaceSize, CubeFace face)
  : maxNodeLevel_(log2(kFaceSize) - Settings::kNodeDimensionExp)
  , root_(kFaceSize/2, kFaceSize/2, face, maxNodeLevel_) {}

void CdlodQuadTree::SelectNodes(const engine::Camera& cam, QuadGridMesh& mesh) {
  root_.selectNodes(cam.transform()->pos(), cam.frustum(), mesh);
  root_.age();
}

