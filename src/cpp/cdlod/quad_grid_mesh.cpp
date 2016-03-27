#include "cdlod/quad_grid_mesh.hpp"

QuadGridMesh::QuadGridMesh(int dimension) : mesh_(dimension/2) {
  assert(2 <= dimension && dimension <= 256);
}

// Adds a subquad to the render list.
// tl = top left, br = bottom right
void QuadGridMesh::addToRenderList(float offset_x, float offset_y,
                                   int level, int face,
                                   bool tl, bool tr, bool bl, bool br) {
  glm::vec4 renderData(offset_x, offset_y, level, face);
  float dim4 = pow(2, level) * mesh_.dimension()/2; // our dimension / 4
  if (tl) {
    mesh_.addToRenderList(renderData + glm::vec4(-dim4, dim4, 0, 0));
  }
  if (tr) {
    mesh_.addToRenderList(renderData + glm::vec4(dim4, dim4, 0, 0));
  }
  if (bl) {
    mesh_.addToRenderList(renderData + glm::vec4(-dim4, -dim4, 0, 0));
  }
  if (br) {
    mesh_.addToRenderList(renderData + glm::vec4(dim4, -dim4, 0, 0));
  }
}

// Adds all four subquads
void QuadGridMesh::addToRenderList(float offset_x, float offset_y,
                                   int level, int face) {
  addToRenderList(offset_x, offset_y, level, face,
                  true, true, true, true);
}

void QuadGridMesh::clearRenderList() {
  mesh_.clearRenderList();
}

// void QuadGridMesh::render() {
//   mesh_.render();
// }

size_t QuadGridMesh::node_count() const {
  return mesh_.node_count();
}
