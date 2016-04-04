// Copyright (c) 2016, Tamas Csala

#include "common/debug_callback.hpp"

#if VK_VALIDATE
#include <iostream>
#include "common/error_checking.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallbackFunction(
  VkDebugReportFlagsEXT       flags,
  VkDebugReportObjectTypeEXT  object_type,
  uint64_t                    object,
  size_t                      location,
  int32_t                     message_code,
  const char*                 layer_prefix,
  const char*                 message,
  void*                       user_data)
{
  std::cerr << (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ? "ERROR " : "WARNING ")
            << "(layer = " << layer_prefix << ", code = " << message_code << ") : " << message << std::endl;

  return VK_FALSE;
}

DebugCallback::DebugCallback(const vk::Instance& instance) : instance_(instance) {
  PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback;
  create_debug_report_callback = (PFN_vkCreateDebugReportCallbackEXT)
    instance.getProcAddr("vkCreateDebugReportCallbackEXT");

  if (!create_debug_report_callback) {
    throw std::runtime_error("GetProcAddr: Unable to find vkCreateDebugReportCallbackEXT");
  }

  vk::DebugReportCallbackCreateInfoEXT dbg_create_info{
      vk::DebugReportFlagBitsEXT::eError |
      vk::DebugReportFlagBitsEXT::eWarning,
      DebugCallbackFunction, nullptr};
  VkDebugReportCallbackCreateInfoEXT vkDbgCreateInfo = dbg_create_info;

  VkChk(create_debug_report_callback(instance, &vkDbgCreateInfo,
                                     nullptr, &msg_callback_));
}

DebugCallback::~DebugCallback() {
  PFN_vkDestroyDebugReportCallbackEXT destroy_debug_report_callback;
  destroy_debug_report_callback = (PFN_vkDestroyDebugReportCallbackEXT)
    instance_.getProcAddr("vkDestroyDebugReportCallbackEXT");

  if (!destroy_debug_report_callback) {
    throw std::runtime_error("GetProcAddr: Unable to find vkDestroyDebugReportCallbackEXT");
  }

  destroy_debug_report_callback(instance_, msg_callback_, nullptr);
}

#endif
