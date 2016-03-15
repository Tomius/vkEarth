#ifndef CREATE_DEBUG_CALLBACK_HPP_
#define CREATE_DEBUG_CALLBACK_HPP_

#include <vulkan/vk_cpp.h>
#include "common/defines.hpp"

namespace Initialize {

#if VK_VALIDATE
class DebugCallback {
public:
  DebugCallback(const vk::Instance& instance);
  ~DebugCallback();

private:
  const vk::Instance& instance_;
  VkDebugReportCallbackEXT msgCallback_ = nullptr;
};
#endif

}

#endif
