// Copyright (c) 2016, Tamas Csala

#include <cassert>
#include <stdexcept>
#include "cdlod/texture_handler.hpp"

TextureHandler::TextureHandler() {
  for (size_t i = 0; i < Settings::kMaxTextureCount; ++i) {
    unused_indices_.insert(i);
  }
}

size_t TextureHandler::GetFirstUnusedTextureIndex() {
  assert(!unused_indices_.empty());
  return *unused_indices_.begin();
}
