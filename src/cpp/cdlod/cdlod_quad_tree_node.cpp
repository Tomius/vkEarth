// Copyright (c) 2016, Tamas Csala

#include <cstring>
#include <algorithm>
#include <lodepng.h>

#include "cdlod/cdlod_quad_tree_node.hpp"
#include "collision/cube2sphere.hpp"

CdlodQuadTreeNode::CdlodQuadTreeNode(double x, double z, CubeFace face,
                                     int level, CdlodQuadTreeNode* parent)
    : x_(x), z_(z), face_(face), level_(level), parent_{parent}
{
  CalculateMinMax();
  RefreshMinMax();
}

void CdlodQuadTreeNode::InitChild(int i) {
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

void CdlodQuadTreeNode::SelectNodes(const glm::vec3& cam_pos,
                                    const Frustum& frustum,
                                    QuadGridMesh& grid_mesh,
                                    ThreadPool& thread_pool,
                                    TextureHandler& texture_handler) {
  last_used_ = 0;

  StreamedTextureInfo texinfo;
  SelectTexture(cam_pos, frustum, thread_pool, texinfo, texture_handler);

  // textures should be loaded, even if it is outside the frustum (otherwise the
  // texture lod difference of neighbour nodes can cause geometry cracks)
  // if (!bbox_.CollidesWithFrustum(frustum)) { return; }

  // If we can cover the whole area or if we are a leaf
  Sphere sphere{cam_pos, Settings::kSmallestGeometryLodDistance * scale()};
  if (!bbox_.CollidesWithSphere(sphere) ||
      level_ <= Settings::kLevelOffset - Settings::kGeomDiv) {
    if (bbox_.CollidesWithFrustum(frustum)) {
      grid_mesh.AddToRenderList(x_, z_, level_, int(face_), texinfo);
    }
  } else {
    bool cc[4]{}; // children collision

    for (int i = 0; i < 4; ++i) {
      if (!children_[i])
        InitChild(i);

      cc[i] = children_[i]->CollidesWithSphere(sphere);
      if (cc[i]) {
        // Ask child to render what we can't
        children_[i]->SelectNodes(cam_pos, frustum, grid_mesh,
                                  thread_pool, texture_handler);
      }
    }

    if (bbox_.CollidesWithFrustum(frustum)) {
      // Render what the children didn't do
      grid_mesh.AddToRenderList(x_, z_, level_, int(face_), texinfo,
                                !cc[0], !cc[1], !cc[2], !cc[3]);
    }
  }
}

void CdlodQuadTreeNode::SelectTexture(const glm::vec3& cam_pos,
                                      const Frustum& frustum,
                                      ThreadPool& thread_pool,
                                      StreamedTextureInfo& texinfo,
                                      TextureHandler& texture_handler,
                                      int recursion_level /*= 0*/) {
  bool need_geometry = (texinfo.geometry_current.size == 0);
  bool need_normal   = (texinfo.normal_current.size == 0);
  bool need_diffuse  = (texinfo.diffuse_current.size == 0);

  // nothing more to find
  if (!need_geometry && !need_normal && !need_diffuse) {
    return;
  }

  if (parent_ == nullptr) {
    if (!texture_.is_loaded_to_gpu) {
      LoadTexture(true);
      Upload(texture_handler);
    }

    if (need_geometry) {
      texinfo.geometry_current = texture_.elevation;
      texinfo.geometry_next = texture_.elevation;
    }
    if (need_normal) {
      texinfo.normal_current = texture_.elevation;
      texinfo.normal_next = texture_.elevation;
    }
    if (need_diffuse) {
      texinfo.diffuse_current = texture_.diffuse;
      texinfo.diffuse_next = texture_.diffuse;
    }

    return;
  }

  bool too_detailed = recursion_level < Settings::kTexDimOffset - Settings::kNormalToGeometryLevelOffset;
  bool too_detailed_for_geometry = recursion_level < Settings::kTexDimOffset;

  bool can_use_geometry = need_geometry && HasElevationTexture() && !too_detailed_for_geometry;
  bool can_use_normal = need_normal && HasElevationTexture() && !too_detailed;
  bool can_use_diffuse = need_diffuse && HasDiffuseTexture() && !too_detailed;

  if (can_use_geometry || can_use_normal || can_use_diffuse) {
    if (!texture_.is_loaded_to_gpu && texture_.is_loaded_to_memory) {
      Upload(texture_handler);
    }

    if (texture_.is_loaded_to_gpu) {
      assert(parent_->texture_.is_loaded_to_gpu);

      if (can_use_geometry) {
        texinfo.geometry_current = texture_.elevation;
        if (recursion_level == Settings::kTexDimOffset) {
          texinfo.geometry_next = parent_->texture_.elevation;
        } else {
          texinfo.geometry_next = texture_.elevation;
        }
      }

      if (can_use_normal) {
        texinfo.normal_current = texture_.elevation;
        texinfo.normal_next = parent_->texture_.elevation;
      }

      if (can_use_diffuse) {
        texinfo.diffuse_current = texture_.diffuse;
        texinfo.diffuse_next = parent_->texture_.diffuse;
      }
    } else {
      // this one should be used, but not yet loaded -> start async load, but
      // make do with the parent for now.
      thread_pool.Enqueue(level_, [this](){ LoadTexture(false); });
    }
  }

  parent_->SelectTexture(cam_pos, frustum, thread_pool,
                         texinfo, texture_handler, recursion_level+1);
}

void CdlodQuadTreeNode::Age(TextureHandler& texture_handler) {
  last_used_++;

  for (auto& child : children_) {
    if (child) {
      // unload child if its age would exceed the ttl
      if (child->last_used_ > kTimeToLiveInMemory) {
        if (child->texture_.is_loaded_to_gpu) {
          if (child->HasElevationTexture()) {
            texture_handler.FreeTexture(child->texture_.elevation.id);
          }
          if (child->HasDiffuseTexture()) {
            texture_handler.FreeTexture(child->texture_.diffuse.id);
          }
        }
        child.reset();
      } else {
        child->Age(texture_handler);
      }
    }
  }
}

bool CdlodQuadTreeNode::CollidesWithSphere(const Sphere& sphere) const {
  return bbox_.CollidesWithSphere(sphere);
}

std::string CdlodQuadTreeNode::GetDiffuseMapPath() const {
  assert(HasDiffuseTexture());
  return std::string{"/home/icecool/projects/C++/OpenGL/ReLoEd/src/resources/textures/diffuse"}
         + "/" + std::to_string(int(face_))
         + "/" + std::to_string(DiffuseTextureLevel())
         + "/" + std::to_string(long(x_) >> Settings::kDiffuseToElevationLevelOffset)
         + "/" + std::to_string(long(z_) >> Settings::kDiffuseToElevationLevelOffset)
         + ".png";
}


std::string CdlodQuadTreeNode::GetHeightMapPath() const {
  assert(HasElevationTexture());
  return std::string{"/media/icecool/SSData/gmted2010_75_cube"}
         + "/" + std::to_string(int(face_))
         + "/" + std::to_string(ElevationTextureLevel())
         + "/" + std::to_string(long(x_))
         + "/" + std::to_string(long(z_))
         + ".png";
}

int CdlodQuadTreeNode::ElevationTextureLevel() const {
  return level_ - Settings::kTexDimOffset;
}

int CdlodQuadTreeNode::DiffuseTextureLevel() const {
  return ElevationTextureLevel() - Settings::kDiffuseToElevationLevelOffset;
}

bool CdlodQuadTreeNode::HasElevationTexture() const {
  return Settings::kLevelOffset <= ElevationTextureLevel();
}

bool CdlodQuadTreeNode::HasDiffuseTexture() const {
  return Settings::kLevelOffset <= DiffuseTextureLevel();
}



static void BinarySwap(std::vector<unsigned char>& data) {
  assert(data.size() % 2 == 0);
  for (int i = 0; i < data.size() / 2; ++i) {
    std::swap(data[2*i], data[2*i + 1]);
  }
}

void CdlodQuadTreeNode::LoadTexture(bool synchronous_load) {
  if (parent_ && !parent_->texture_.is_loaded_to_memory) {
    parent_->LoadTexture(true);
  }
  if (texture_.is_loaded_to_memory) {
    return;
  }

  if (synchronous_load) {
    texture_.load_mutex.lock();
  } else {
    if (!texture_.load_mutex.try_lock()) {
      // someone is already loading this texture, so we should not wait on it
      return;
    }
  }

  if (!texture_.is_loaded_to_memory) {
    if (HasElevationTexture()) {
      unsigned width, height;
      std::vector<unsigned char> data;
      unsigned error = lodepng::decode(data, width, height,
                                       GetHeightMapPath(), LCT_RGBA, 16);
      // TODO: IT'S NOT RGBA!!!
      assert(data.size() == 8*width*height);
      BinarySwap(data);
      if (error) {
        std::cerr << "Image decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
        std::terminate();
      }

      assert(width == Settings::kElevationTexSizeWithBorders);
      assert(height == Settings::kElevationTexSizeWithBorders);

      size_t size_in_elements = data.size() / sizeof(texture_.elevation_data[0]);
      texture_.elevation_data.resize(size_in_elements);
      std::memcpy(texture_.elevation_data.data(), data.data(), data.size());

      CalculateMinMax();
    }

    if (HasDiffuseTexture()) {
      unsigned width, height;
      std::vector<unsigned char> data;
      unsigned error = lodepng::decode(data, width, height,
                                       GetDiffuseMapPath(), LCT_RGBA, 16);
      // TODO: IT'S NOT 16 bit!!!
      BinarySwap(data);
      if (error) {
        std::cerr << "Image decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
        std::terminate();
      }

      assert(width == Settings::kDiffuseTexSizeWithBorders);
      assert(height == Settings::kDiffuseTexSizeWithBorders);

      size_t size_in_elements = data.size() / sizeof(texture_.diffuse_data[0]);
      texture_.diffuse_data.resize(size_in_elements);
      std::memcpy(texture_.diffuse_data.data(), data.data(), data.size());
    }

    texture_.is_loaded_to_memory = true;
  }

  texture_.load_mutex.unlock();
}

void CdlodQuadTreeNode::Upload(TextureHandler& texture_handler) {
  LoadTexture(true);
  if (parent_ && !parent_->texture_.is_loaded_to_gpu) {
    parent_->Upload(texture_handler);
  }

  if (!texture_.is_loaded_to_gpu) {
    if (HasElevationTexture()) {
      RefreshMinMax();
      texture_.elevation.id = texture_handler.GetFirstUnusedTextureIndex();
      texture_handler.SetupTexture(texture_.elevation.id,
                                   Settings::kElevationTexSizeWithBorders,
                                   Settings::kElevationTexSizeWithBorders,
                                   (const unsigned char*)texture_.elevation_data.data());
      double scale = static_cast<double>(Settings::kElevationTexSizeWithBorders)
                   / static_cast<double>(Settings::kTextureDimension);
      texture_.elevation.size = scale * size();
      texture_.elevation.position = glm::vec2(x_ - texture_.elevation.size/2,
                                              z_ - texture_.elevation.size/2);

      texture_.elevation_data.clear();
    }

    if (HasDiffuseTexture()) {
      texture_.diffuse.id = texture_handler.GetFirstUnusedTextureIndex();
      texture_handler.SetupTexture(texture_.diffuse.id,
                                   Settings::kDiffuseTexSizeWithBorders,
                                   Settings::kDiffuseTexSizeWithBorders,
                                   (const unsigned char*)texture_.diffuse_data.data());
      double scale = static_cast<double>(Settings::kDiffuseTexSizeWithBorders)
                   / static_cast<double>(Settings::kTextureDimension);
      texture_.diffuse.size = scale * size();
      texture_.diffuse.position = glm::vec2(x_ - texture_.diffuse.size/2,
                                            z_ - texture_.diffuse.size/2);
      texture_.diffuse_data.clear();
    }

    texture_.is_loaded_to_gpu = true;
  }
}

void CdlodQuadTreeNode::CalculateMinMax() {
  if (!texture_.elevation_data.empty()) {
    texture_.min_max_src = this;
  } else if (parent_ && parent_->texture_.min_max_src) {
    texture_.min_max_src = parent_->texture_.min_max_src;
  } else {
    return; // no elevation info from the parents -> nothing to do
  }

  auto& src = texture_.min_max_src;
  int texSize = Settings::kTextureDimension;
  int texSizeWBorder = Settings::kElevationTexSizeWithBorders;

  double scale = static_cast<double>(texSizeWBorder) / texSize;
  double src_size = src->size() * scale;
  glm::dvec2 src_center = glm::dvec2{src->x_, src->z_};
  glm::dvec2 src_min = src_center - src_size/2.0;

  glm::dvec2 this_min {x_ - size()/2.0, z_ - size()/2.0};
  glm::dvec2 this_max {x_ + size()/2.0, z_ + size()/2.0};

  double src_to_tex_scale = texSizeWBorder / src_size;
  glm::ivec2 min_coord = glm::ivec2(floor((this_min - src_min) * src_to_tex_scale));
  glm::ivec2 max_coord = glm::ivec2(ceil ((this_max - src_min) * src_to_tex_scale));

  texture_.min = std::numeric_limits<uint16_t>::max();
  texture_.max = std::numeric_limits<uint16_t>::min();

  auto& data = src->texture_.elevation_data;
  for (int x = min_coord.x; x < max_coord.x; ++x) {
    for (int y = min_coord.y; y < max_coord.y; ++y) {
      uint16_t height = data[(y*texSizeWBorder + x) * 4];
      texture_.min = std::min(texture_.min, height);
      texture_.max = std::max(texture_.max, height);
    }
  }

  texture_.min_h = texture_.min * Settings::kMaxHeight /
                   std::numeric_limits<uint16_t>::max();
  texture_.max_h = texture_.max * Settings::kMaxHeight /
                   std::numeric_limits<uint16_t>::max();

  for (auto& child : children_) {
    if (child && !child->texture_.is_loaded_to_memory) {
      child->CalculateMinMax();
    }
  }
}

void CdlodQuadTreeNode::RefreshMinMax() {
  bbox_ = SpherizedAABBDivided{
    {x_-size()/2, texture_.min_h, z_-size()/2},
    {x_+size()/2, texture_.max_h, z_+size()/2},
  face_, Settings::kFaceSize};

  for (auto& child : children_) {
    if (child && !child->texture_.is_loaded_to_memory) {
      child->RefreshMinMax();
    }
  }
}
