// Copyright (c) 2016, Tamas Csala

#include "cdlod/grid_mesh.hpp"

uint16_t GridMesh::IndexOf(int x, int y) {
  x += dimension_/2;
  y += dimension_/2;
  return (dimension_ + 1) * y + x;
}

GridMesh::GridMesh(uint8_t dimension) : dimension_(dimension) {
  positions_.reserve((dimension_+1) * (dimension_+1));

  uint8_t dim2 = dimension_/2;

  for (int y = -dim2; y <= dim2; ++y) {
    for (int x = -dim2; x <= dim2; ++x) {
      positions_.push_back(svec2(x, y));
    }
  }

  index_count_ = 6*dimension_*dimension_;
  indices_.reserve(index_count_);

  for (int y = -dim2; y < dim2; ++y) {
    for (int x = -dim2; x < dim2; ++x) {
      indices_.push_back(IndexOf(x, y));
      indices_.push_back(IndexOf(x, y+1));
      indices_.push_back(IndexOf(x+1, y));

      indices_.push_back(IndexOf(x+1, y));
      indices_.push_back(IndexOf(x, y+1));
      indices_.push_back(IndexOf(x+1, y+1));
    }
  }

  assert(index_count_ == indices_.size());
}

void GridMesh::AddToRenderList(const glm::vec4& renderData) {
  renderData_.push_back(renderData);
}

void GridMesh::ClearRenderList() {
  renderData_.clear();
}

