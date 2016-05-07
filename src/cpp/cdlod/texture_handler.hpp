// Copyright (c) 2016, Tamas Csala

#ifndef TEXTURE_HANDLER_HPP_
#define TEXTURE_HANDLER_HPP_

#include <set>
#include <vulkan/vk_cpp.hpp>
#include "common/settings.hpp"

class TextureHandler {
 public:
  TextureHandler();
  size_t GetFirstUnusedTextureIndex();
  virtual void SetupTexture(size_t index, unsigned width, unsigned height,
                            vk::Format format, size_t byte_per_texel,
                            const unsigned char* data) = 0;
  virtual void FreeTexture(size_t index) = 0;

 protected:
  std::set<size_t> unused_indices_;
  size_t used_index_count_ = 0;
  vk::DescriptorImageInfo tex_descriptor_infos_[Settings::kMaxTextureCount];
};

#endif
