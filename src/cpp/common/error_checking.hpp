// Copyright (c) 2016, Tamas Csala

#ifndef ERROR_CHECKING_HPP_
#define ERROR_CHECKING_HPP_

#include <stdexcept>
#include <vulkan/vk_cpp.h>

struct VulkanError : public std::runtime_error {
  VkResult result;

  VulkanError(VkResult result);
};

inline void VkChk(VkResult result) {
  if (result != VK_SUCCESS) {
    throw VulkanError(result);
  }
}

namespace vk {
  inline void chk(vk::Result result) {
    if (result != vk::Result::eSuccess) {
      throw VulkanError(static_cast<VkResult>(result));
    }
  }
}

#endif // ERROR_CHECKING_HPP_
