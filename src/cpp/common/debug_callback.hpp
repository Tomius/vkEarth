// Copyright (c) 2016, Tamas Csala

#ifndef COMMON_DEBUG_CALLBACK_HPP_
#define COMMON_DEBUG_CALLBACK_HPP_

#include <vulkan/vk_cpp.h>
#include "common/settings.hpp"

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

#endif
