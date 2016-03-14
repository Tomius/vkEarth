#ifndef VALIDATION_LAYER_CHECKER_HPP_
#define VALIDATION_LAYER_CHECKER_HPP_

#include <vulkan/vk_cpp.h>
#include "common/defines.hpp"

namespace Initialize {

#if VK_VALIDATE
bool CheckForMissingLayers(uint32_t check_count,
                           const char* const* check_names,
                           uint32_t layer_count,
                           vk::LayerProperties *layers);
#endif // VK_VALIDATE

}

#endif // VALIDATION_LAYER_CHECKER_HPP_
