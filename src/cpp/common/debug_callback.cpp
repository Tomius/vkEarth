// Copyright (c) 2016, Tamas Csala

#include "common/debug_callback.hpp"

#if VK_VALIDATE
#include <iostream>
#include "common/error_checking.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallbackFunction(
  VkDebugReportFlagsEXT       flags,
  VkDebugReportObjectTypeEXT  objectType,
  uint64_t                    object,
  size_t                      location,
  int32_t                     messageCode,
  const char*                 pLayerPrefix,
  const char*                 pMessage,
  void*                       pUserData)
{
  std::cerr << (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ? "ERROR " : "WARNING ")
            << "(layer = " << pLayerPrefix << ", code = " << messageCode << ") : " << pMessage << std::endl;

  return VK_FALSE;
}

DebugCallback::DebugCallback(const vk::Instance& instance) : instance_(instance) {
  PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback;
  createDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)
    instance.getProcAddr("vkCreateDebugReportCallbackEXT");

  if (!createDebugReportCallback) {
    throw std::runtime_error("GetProcAddr: Unable to find vkCreateDebugReportCallbackEXT");
  }

  vk::DebugReportCallbackCreateInfoEXT dbgCreateInfo{
      vk::DebugReportFlagBitsEXT::eError |
      vk::DebugReportFlagBitsEXT::eWarning,
      DebugCallbackFunction, nullptr};
  VkDebugReportCallbackCreateInfoEXT vkDbgCreateInfo = dbgCreateInfo;

  VkChk(createDebugReportCallback(instance, &vkDbgCreateInfo,
                                  nullptr, &msgCallback_));
}

DebugCallback::~DebugCallback() {
  PFN_vkDestroyDebugReportCallbackEXT destroyDebugReportCallback;
  destroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)
    instance_.getProcAddr("vkDestroyDebugReportCallbackEXT");

  if (!destroyDebugReportCallback) {
    throw std::runtime_error("GetProcAddr: Unable to find vkDestroyDebugReportCallbackEXT");
  }

  destroyDebugReportCallback(instance_, msgCallback_, nullptr);
}

#endif
