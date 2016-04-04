// Copyright (c) 2016, Tamas Csala

#include "cdlod/quad_grid_mesh.hpp"

QuadGridMesh::QuadGridMesh(int dimension) : mesh_(dimension/2) {
  assert(2 <= dimension && dimension <= 256);
}

// Adds a subquad to the render list.
// tl = top left, br = bottom right
void QuadGridMesh::AddToRenderList(float offset_x, float offset_y,
                                   int level, int face,
                                   bool tl, bool tr, bool bl, bool br) {
  glm::vec4 render_data(offset_x, offset_y, level, face);
  float dim4 = pow(2, level) * mesh_.dimension()/2; // our dimension / 4
  if (tl) {
    mesh_.AddToRenderList(render_data + glm::vec4(-dim4, dim4, 0, 0));
  }
  if (tr) {
    mesh_.AddToRenderList(render_data + glm::vec4(dim4, dim4, 0, 0));
  }
  if (bl) {
    mesh_.AddToRenderList(render_data + glm::vec4(-dim4, -dim4, 0, 0));
  }
  if (br) {
    mesh_.AddToRenderList(render_data + glm::vec4(dim4, -dim4, 0, 0));
  }
}

// Adds all four subquads
void QuadGridMesh::AddToRenderList(float offset_x, float offset_y,
                                   int level, int face) {
  AddToRenderList(offset_x, offset_y, level, face,
                  true, true, true, true);
}

void QuadGridMesh::ClearRenderList() {
  mesh_.ClearRenderList();
}

// void QuadGridMesh::render() {
//   mesh_.render();
// }

size_t QuadGridMesh::node_count() const {
  return mesh_.node_count();
}
