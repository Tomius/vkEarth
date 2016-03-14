#include "initialize/validation_layer_checker.hpp"

#include <iostream>

#if VK_VALIDATE
bool Initialize::CheckForMissingLayers(uint32_t check_count,
                                       const char* const* check_names,
                                       uint32_t layer_count,
                                       vk::LayerProperties *layers) {
  bool found_all = true;
  for (uint32_t i = 0; i < check_count; i++) {
    vk::Bool32 found = 0;
    for (uint32_t j = 0; j < layer_count; j++) {
      if (!strcmp(check_names[i], layers[j].layerName())) {
        found = 1;
        break;
      }
    }
    if (!found) {
      std::cerr << "Cannot find layer: " << check_names[i] << std::endl;
      found_all = false;
    }
  }

  return found_all;
}
#endif // VK_VALIDATE
