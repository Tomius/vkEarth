/*
 * Copyright (c) 2015-2016 The KHRonos Group Inc.
 * Copyright (c) 2015-2016 Valve Corporation
 * Copyright (c) 2015-2016 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and/or associated documentation files (the "Materials"), to
 * deal in the Materials without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Materials, and to permit persons to whom the Materials are
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included in
 * all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE MATERIALS OR THE
 * USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 * Author: Chia-I Wu <olvaffe@gmail.com>
 * Author: Cody Northrop <cody@lunarg.com>
 * Author: Courtney Goeltzenleuchter <courtney@LunarG.com>
 * Author: Ian Elliott <ian@LunarG.com>
 * Author: Jon Ashburn <jon@lunarg.com>
 * Author: Piers Daniell <pdaniell@nvidia.com>
 */
/*
 * Draw a textured triangle with depth testing.  This is written against Intel
 * ICD.  It does not do state transition nor object memory binding like it
 * should.  It also does no error checking.
 */

#ifndef _MSC_VER
#define _ISOC11_SOURCE /* for aligned_alloc() */
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cassert>
#include <iostream>

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "shader/glsl2spv.hpp"
#include "initialize/create_pipeline.hpp"

#define DEMO_TEXTURE_COUNT 1
#define VERTEX_BUFFER_BIND_ID 0
#define APP_SHORT_NAME "vkEarth"
#define APP_LONG_NAME "Vulkan planetary CDLOD"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#if defined(NDEBUG) && defined(__GNUC__)
  #define U_ASSERT_ONLY __attribute__((unused))
#else
  #define U_ASSERT_ONLY
#endif

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                                                   \
    {                                                                                              \
        demo->fp##entrypoint =                                                                     \
            (PFN_vk##entrypoint)inst.getProcAddr("vk" #entrypoint);                                \
        if (demo->fp##entrypoint == nullptr) {                                                     \
           throw std::runtime_error("vk::getInstanceProcAddr failed to find vk" #entrypoint);      \
        }                                                                                          \
    }

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                                                      \
    {                                                                                              \
        demo->fp##entrypoint =                                                                     \
            (PFN_vk##entrypoint)dev.getProcAddr("vk" #entrypoint);                                 \
        if (demo->fp##entrypoint == nullptr) {                                                     \
            throw std::runtime_error("vk::getDeviceProcAddr failed to find vk" #entrypoint);       \
        }                                                                                          \
    }

struct texture_object {
    vk::Sampler sampler;

    vk::Image image;
    vk::ImageLayout imageLayout;

    vk::DeviceMemory mem;
    vk::ImageView view;
    int32_t tex_width = 0, tex_height = 0;
};

VKAPI_ATTR VkBool32 VKAPI_CALL dbgFunc(
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

typedef struct _SwapchainBuffers {
    vk::Image image;
    vk::CommandBuffer cmd;
    vk::ImageView view;
} SwapchainBuffers;


struct Demo {
    GLFWwindow* window;
    VkSurfaceKHR surface;
    bool use_staging_buffer = false;

    vk::Instance inst;
    vk::PhysicalDevice gpu;
    vk::Device device;
    vk::Queue queue;
    vk::PhysicalDeviceProperties gpu_props;
    vk::QueueFamilyProperties *queue_props;
    uint32_t graphics_queue_node_index = 0;

    uint32_t enabled_extension_count = 0;
    uint32_t enabled_layer_count = 0;
    std::string extension_names[64];
    const char* device_validation_layers[64];

    int width = 0, height = 0;
    vk::Format format;
    vk::ColorSpaceKHR color_space;

    PFN_vkGetPhysicalDeviceSurfaceSupportKHR
        fpGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        fpGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        fpGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        fpGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR fpQueuePresentKHR = nullptr;
    uint32_t swapchainImageCount = 0;
    vk::SwapchainKHR swapchain;
    SwapchainBuffers *buffers = nullptr;

    vk::CommandPool cmd_pool;

    struct {
        vk::Format format;

        vk::Image image;
        vk::DeviceMemory mem;
        vk::ImageView view;
    } depth;

    struct texture_object textures[DEMO_TEXTURE_COUNT];

    struct {
        vk::Buffer buf;
        vk::DeviceMemory mem;

        vk::PipelineVertexInputStateCreateInfo vi;
        vk::VertexInputBindingDescription vi_bindings[1];
        vk::VertexInputAttributeDescription vi_attrs[2];
    } vertices;

    vk::CommandBuffer setup_cmd; // Command Buffer for initialization commands
    vk::CommandBuffer draw_cmd;  // Command Buffer for drawing commands
    vk::PipelineLayout pipeline_layout;
    vk::DescriptorSetLayout desc_layout;
    vk::RenderPass render_pass;
    vk::Pipeline pipeline;

    vk::DescriptorPool desc_pool;
    vk::DescriptorSet desc_set;

    vk::Framebuffer *framebuffers = nullptr;

    vk::PhysicalDeviceMemoryProperties memory_properties;

    bool validate = true;
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback;
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback;
    VkDebugReportCallbackEXT msg_callback;

    float depthStencil = 0.0f;
    float depthIncrement = 0.0f;

    uint32_t current_buffer = 0;
    uint32_t queue_count = 0;
};

// Forward declaration:
static void demo_resize(Demo *demo);

static bool memory_type_from_properties(Demo *demo, uint32_t typeBits,
                                        vk::MemoryPropertyFlags requirements_mask,
                                        vk::MemoryAllocateInfo& mem_alloc) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((demo->memory_properties.memoryTypes()[i].propertyFlags() &
                 requirements_mask) == requirements_mask) {
                mem_alloc.memoryTypeIndex(i);
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

static void demo_flush_init_cmd(Demo *demo) {
    vk::Result U_ASSERT_ONLY err;

    if (demo->setup_cmd == VK_NULL_HANDLE)
        return;

    err = demo->setup_cmd.end();
    assert(err == vk::Result::eSuccess);

    const vk::CommandBuffer cmd_bufs[] = {demo->setup_cmd};
    vk::Fence nullFence = {VK_NULL_HANDLE};
    vk::SubmitInfo submit_info = vk::SubmitInfo()
                                 .commandBufferCount(1)
                                 .pCommandBuffers(cmd_bufs);

    err = demo->queue.submit(1, &submit_info, nullFence);
    assert(err == vk::Result::eSuccess);

    err = demo->queue.waitIdle();
    assert(err == vk::Result::eSuccess);

    demo->device.freeCommandBuffers(demo->cmd_pool, 1, cmd_bufs);
    demo->setup_cmd = VK_NULL_HANDLE;
}

static void demo_set_image_layout(Demo *demo, vk::Image image,
                                  vk::ImageAspectFlags aspectMask,
                                  vk::ImageLayout old_image_layout,
                                  vk::ImageLayout new_image_layout) {
    vk::Result U_ASSERT_ONLY err;

    if (demo->setup_cmd == VK_NULL_HANDLE) {
        const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
            .commandPool(demo->cmd_pool)
            .level(vk::CommandBufferLevel::ePrimary)
            .commandBufferCount(1);

        err = demo->device.allocateCommandBuffers(&cmd, &demo->setup_cmd);
        assert(err == vk::Result::eSuccess);

        vk::CommandBufferInheritanceInfo cmd_buf_hinfo =
            vk::CommandBufferInheritanceInfo();

        vk::CommandBufferBeginInfo cmd_buf_info =
          vk::CommandBufferBeginInfo().pInheritanceInfo(&cmd_buf_hinfo);

        err = demo->setup_cmd.begin(&cmd_buf_info);
        assert(err == vk::Result::eSuccess);
    }

    vk::ImageMemoryBarrier image_memory_barrier = vk::ImageMemoryBarrier()
        .oldLayout(old_image_layout)
        .newLayout(new_image_layout)
        .image(image)
        .subresourceRange({aspectMask, 0, 1, 0, 1});

    if (new_image_layout == vk::ImageLayout::eTransferDstOptimal) {
        /* Make sure anything that was copying from this image has completed */
        image_memory_barrier.dstAccessMask(vk::AccessFlagBits::eTransferRead);
    }

    if (new_image_layout == vk::ImageLayout::eColorAttachmentOptimal) {
        image_memory_barrier.dstAccessMask(
            vk::AccessFlagBits::eColorAttachmentWrite);
    }

    if (new_image_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        image_memory_barrier.dstAccessMask(
            vk::AccessFlagBits::eDepthStencilAttachmentWrite);
    }

    if (new_image_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        /* Make sure any Copy or CPU writes to image are flushed */
        image_memory_barrier.dstAccessMask(vk::AccessFlagBits::eShaderRead |
            vk::AccessFlagBits::eInputAttachmentRead);
    }

    vk::ImageMemoryBarrier *pmemory_barrier = &image_memory_barrier;

    vk::PipelineStageFlags src_stages = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags dest_stages = vk::PipelineStageFlagBits::eTopOfPipe;

    demo->setup_cmd.pipelineBarrier(src_stages, dest_stages,
      vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, pmemory_barrier);
}

static void demo_draw_build_cmd(Demo *demo) {
    const vk::CommandBufferInheritanceInfo cmd_buf_hinfo;
    const vk::CommandBufferBeginInfo cmd_buf_info =
        vk::CommandBufferBeginInfo().pInheritanceInfo(&cmd_buf_hinfo);
    const vk::ClearValue clear_values[2] = {
        vk::ClearValue().color(
            vk::ClearColorValue{std::array<float,4>{0.2f, 0.2f, 0.2f, 0.2f}}),
        vk::ClearValue().depthStencil(
            vk::ClearDepthStencilValue{demo->depthStencil, 0})
    };
    const vk::RenderPassBeginInfo rp_begin = vk::RenderPassBeginInfo()
        .renderPass(demo->render_pass)
        .framebuffer(demo->framebuffers[demo->current_buffer])
        .renderArea({vk::Offset2D(0, 0), vk::Extent2D(demo->width, demo->height)})
        .clearValueCount(2)
        .pClearValues(clear_values);
    vk::Result U_ASSERT_ONLY err;

    err = demo->draw_cmd.begin(&cmd_buf_info);
    assert(err == vk::Result::eSuccess);

    demo->draw_cmd.beginRenderPass(&rp_begin, vk::SubpassContents::eInline);
    demo->draw_cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, demo->pipeline);
    demo->draw_cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        demo->pipeline_layout, 0, 1, &demo->desc_set, 0, nullptr);

    vk::Viewport viewport = vk::Viewport()
      .height(demo->height)
      .width(demo->width)
      .minDepth(0.0f)
      .maxDepth(1.0f);
    demo->draw_cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{vk::Offset2D(0, 0), vk::Extent2D(demo->width, demo->height)};
    demo->draw_cmd.setScissor(0, 1, &scissor);

    vk::DeviceSize offsets[1] = {0};
    demo->draw_cmd.bindVertexBuffers(VERTEX_BUFFER_BIND_ID, 1,
                                     &demo->vertices.buf, offsets);

    demo->draw_cmd.draw(3, 1, 0, 0);
    demo->draw_cmd.endRenderPass();

    vk::ImageMemoryBarrier prePresentBarrier = vk::ImageMemoryBarrier()
        .srcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
        .dstAccessMask(vk::AccessFlagBits::eMemoryRead)
        .oldLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .newLayout(vk::ImageLayout::ePresentSrcKHR)
        .srcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .subresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    prePresentBarrier.image(demo->buffers[demo->current_buffer].image);
    vk::ImageMemoryBarrier *pmemory_barrier = &prePresentBarrier;
    demo->draw_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                   vk::PipelineStageFlagBits::eBottomOfPipe,
                                   vk::DependencyFlags(), 0, nullptr, 0,
                                   nullptr, 1, pmemory_barrier);

    err = demo->draw_cmd.end();
    assert(err == vk::Result::eSuccess);
}

static void demo_draw(Demo *demo) {
    vk::Result U_ASSERT_ONLY err;
    VkResult vkErr;
    vk::Semaphore presentCompleteSemaphore;
    vk::SemaphoreCreateInfo presentCompleteSemaphoreCreateInfo;

    err = demo->device.createSemaphore(&presentCompleteSemaphoreCreateInfo,
                                       nullptr, &presentCompleteSemaphore);
    assert(err == vk::Result::eSuccess);

    // Get the index of the next available swapchain image:
    vkErr = demo->fpAcquireNextImageKHR(demo->device, demo->swapchain, UINT64_MAX,
                                        presentCompleteSemaphore,
                                        (vk::Fence)0, // TODO: Show use of fence
                                        &demo->current_buffer);
    if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
        // demo->swapchain is out of date (e.g. the window was resized) and
        // must be recreated:
        demo_resize(demo);
        demo_draw(demo);
        demo->device.destroySemaphore(presentCompleteSemaphore, nullptr);
        return;
    } else if (vkErr == VK_SUBOPTIMAL_KHR) {
        // demo->swapchain is not as optimal as it could be, but the platform's
        // presentation engine will still present the image correctly.
    } else {
        assert(vkErr == VK_SUCCESS);
    }

    // Assume the command buffer has been run on current_buffer before so
    // we need to set the image layout back to COLOR_ATTACHMENT_OPTIMAL
    demo_set_image_layout(demo, demo->buffers[demo->current_buffer].image,
                          vk::ImageAspectFlagBits::eColor,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::ImageLayout::eColorAttachmentOptimal);
    demo_flush_init_cmd(demo);

    // Wait for the present complete semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.

    // FIXME/TODO: DEAL WITH vk::ImageLayout::ePresentSrcKHR
    demo_draw_build_cmd(demo);
    vk::Fence nullFence;
    vk::PipelineStageFlags pipe_stage_flags =
        vk::PipelineStageFlagBits::eBottomOfPipe;
    vk::SubmitInfo submit_info = vk::SubmitInfo()
        .waitSemaphoreCount(1)
        .pWaitSemaphores(&presentCompleteSemaphore)
        .pWaitDstStageMask(&pipe_stage_flags)
        .commandBufferCount(1)
        .pCommandBuffers(&demo->draw_cmd);

    err = demo->queue.submit(1, &submit_info, nullFence);
    assert(err == vk::Result::eSuccess);

    vk::PresentInfoKHR present = vk::PresentInfoKHR()
        .swapchainCount(1)
        .pSwapchains(&demo->swapchain)
        .pImageIndices(&demo->current_buffer);
    VkPresentInfoKHR& vkPresent =
      const_cast<VkPresentInfoKHR&>(
        static_cast<const VkPresentInfoKHR&>(present));

    // TBD/TODO: SHOULD THE "present" PARAMETER BE "const" IN THE HEADER?
    vkErr = demo->fpQueuePresentKHR(demo->queue, &vkPresent);
    if (vkErr == VK_ERROR_OUT_OF_DATE_KHR) {
        // demo->swapchain is out of date (e.g. the window was resized) and
        // must be recreated:
        demo_resize(demo);
    } else if (vkErr == VK_SUBOPTIMAL_KHR) {
        // demo->swapchain is not as optimal as it could be, but the platform's
        // presentation engine will still present the image correctly.
    } else {
        assert(vkErr == VK_SUCCESS);
    }

    err = demo->queue.waitIdle();
    assert(err == vk::Result::eSuccess);

    demo->device.destroySemaphore(presentCompleteSemaphore, nullptr);
}

static void demo_prepare_buffers(Demo *demo) {
    vk::Result U_ASSERT_ONLY err;
    VkResult U_ASSERT_ONLY vkErr;
    vk::SwapchainKHR oldSwapchain = demo->swapchain;

    // Check the surface capabilities and formats
    vk::SurfaceCapabilitiesKHR surfCapabilities;
    VkSurfaceCapabilitiesKHR& vkSurfCapabilities =
      const_cast<VkSurfaceCapabilitiesKHR&>(
        static_cast<const VkSurfaceCapabilitiesKHR&>(surfCapabilities));
    vkErr = demo->fpGetPhysicalDeviceSurfaceCapabilitiesKHR(
        demo->gpu, demo->surface, &vkSurfCapabilities);
    assert(vkErr == VK_SUCCESS);

    uint32_t presentModeCount;
    vkErr = demo->fpGetPhysicalDeviceSurfacePresentModesKHR(
        demo->gpu, demo->surface, &presentModeCount, nullptr);
    assert(vkErr == VK_SUCCESS);

    VkPresentModeKHR *presentModes = new VkPresentModeKHR[presentModeCount];
    assert(presentModes);

    vkErr = demo->fpGetPhysicalDeviceSurfacePresentModesKHR(
        demo->gpu, demo->surface, &presentModeCount, presentModes);
    assert(vkErr == VK_SUCCESS);

    vk::Extent2D swapchainExtent;
    // width and height are either both -1, or both not -1.
    if (surfCapabilities.currentExtent().width() == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width(demo->width);
        swapchainExtent.height(demo->height);
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent();
        demo->width = surfCapabilities.currentExtent().width();
        demo->height = surfCapabilities.currentExtent().height();
    }

    vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifoKHR;

    // Determine the number of vk::Image's to use in the swap chain (we desire to
    // own only 1 image at a time, besides the images being displayed and
    // queued for display):
    uint32_t desiredNumberOfSwapchainImages =
        surfCapabilities.minImageCount() + 1;
    if ((surfCapabilities.maxImageCount() > 0) &&
        (desiredNumberOfSwapchainImages > surfCapabilities.maxImageCount())) {
        // Application must settle for fewer images than desired:
        desiredNumberOfSwapchainImages = surfCapabilities.maxImageCount();
    }

    vk::SurfaceTransformFlagBitsKHR preTransform;
    if (surfCapabilities.supportedTransforms() &
        vk::SurfaceTransformFlagBitsKHR::eIdentity) {
        preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    } else {
        preTransform = surfCapabilities.currentTransform();
    }

    const vk::SwapchainCreateInfoKHR swapchain = vk::SwapchainCreateInfoKHR()
        .surface(demo->surface)
        .minImageCount(desiredNumberOfSwapchainImages)
        .imageFormat(demo->format)
        .imageColorSpace(demo->color_space)
        .imageExtent(vk::Extent2D{swapchainExtent.width(), swapchainExtent.height()})
        .imageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .preTransform(preTransform)
        .compositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .imageArrayLayers(1)
        .imageSharingMode(vk::SharingMode::eExclusive)
        .queueFamilyIndexCount(0)
        .pQueueFamilyIndices(nullptr)
        .presentMode(swapchainPresentMode)
        .oldSwapchain(oldSwapchain)
        .clipped(true);

    const VkSwapchainCreateInfoKHR& vkSwapchainInfo = swapchain;
    VkSwapchainKHR vkSwapchain = demo->swapchain;
    vkErr = demo->fpCreateSwapchainKHR(demo->device, &vkSwapchainInfo, nullptr,
                                       &vkSwapchain);
    demo->swapchain = vkSwapchain;
    assert(vkErr == VK_SUCCESS);

    // If we just re-created an existing swapchain, we should destroy the old
    // swapchain at this point.
    // Note: destroying the swapchain also cleans up all its associated
    // presentable images once the platform is done with them.
    if (oldSwapchain != VK_NULL_HANDLE) {
        demo->fpDestroySwapchainKHR(demo->device, oldSwapchain, nullptr);
    }

    vkErr = demo->fpGetSwapchainImagesKHR(demo->device, demo->swapchain,
                                        &demo->swapchainImageCount, nullptr);
    assert(vkErr == VK_SUCCESS);

    VkImage *swapchainImages = new VkImage[demo->swapchainImageCount];
    vkErr = demo->fpGetSwapchainImagesKHR(demo->device, demo->swapchain,
                                        &demo->swapchainImageCount,
                                        swapchainImages);
    assert(vkErr == VK_SUCCESS);

    demo->buffers = new SwapchainBuffers[demo->swapchainImageCount];

    for (uint32_t i = 0; i < demo->swapchainImageCount; i++) {
        vk::ImageViewCreateInfo color_attachment_view = vk::ImageViewCreateInfo()
            .format(demo->format)
            .subresourceRange(vk::ImageSubresourceRange()
              .aspectMask(vk::ImageAspectFlagBits::eColor)
              .baseMipLevel(0)
              .levelCount(1)
              .baseArrayLayer(0)
              .layerCount(1)
            )
            .viewType(vk::ImageViewType::e2D);

        demo->buffers[i].image = swapchainImages[i];

        // Render loop will expect image to have been used before and in
        // vk::ImageLayout::ePresentSrcKHR
        // layout and will change to COLOR_ATTACHMENT_OPTIMAL, so init the image
        // to that state
        demo_set_image_layout(
            demo, demo->buffers[i].image, vk::ImageAspectFlagBits::eColor,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

        color_attachment_view.image(demo->buffers[i].image);

        err = demo->device.createImageView(&color_attachment_view, nullptr,
                                           &demo->buffers[i].view);
        assert(err == vk::Result::eSuccess);
    }

    demo->current_buffer = 0;

    if (nullptr != presentModes) {
        delete[] presentModes;
    }
}

static void demo_prepare_depth(Demo *demo) {
    const vk::Format depth_format = vk::Format::eD16Unorm;
    const vk::ImageCreateInfo image = vk::ImageCreateInfo()
        .imageType(vk::ImageType::e2D)
        .format(depth_format)
        .extent(vk::Extent3D(demo->width, demo->height, 1))
        .mipLevels(1)
        .arrayLayers(1)
        .samples(vk::SampleCountFlagBits::e1)
        .tiling(vk::ImageTiling::eOptimal)
        .usage(vk::ImageUsageFlagBits::eDepthStencilAttachment);

    vk::MemoryAllocateInfo mem_alloc;
    vk::ImageViewCreateInfo view = vk::ImageViewCreateInfo()
      .format(depth_format)
      .subresourceRange(vk::ImageSubresourceRange()
        .aspectMask(vk::ImageAspectFlagBits::eDepth)
        .baseMipLevel(0)
        .levelCount(1)
        .baseArrayLayer(0)
        .layerCount(1)
      )
      .viewType(vk::ImageViewType::e2D);

    vk::MemoryRequirements mem_reqs;
    vk::Result U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;

    demo->depth.format = depth_format;

    /* create image */
    err = demo->device.createImage(&image, nullptr, &demo->depth.image);
    assert(err == vk::Result::eSuccess);

    /* get memory requirements for this object */
    demo->device.getImageMemoryRequirements(demo->depth.image, &mem_reqs);

    /* select memory size and type */
    mem_alloc.allocationSize(mem_reqs.size());
    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits(),
                                       vk::MemoryPropertyFlags(), /* No requirements */
                                       mem_alloc);
    assert(pass);

    /* allocate memory */
    err = demo->device.allocateMemory(&mem_alloc, nullptr, &demo->depth.mem);
    assert(err == vk::Result::eSuccess);

    /* bind memory */
    err = demo->device.bindImageMemory(demo->depth.image, demo->depth.mem, 0);
    assert(err == vk::Result::eSuccess);

    demo_set_image_layout(demo, demo->depth.image, vk::ImageAspectFlagBits::eDepth,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthStencilAttachmentOptimal);

    /* create image view */
    view.image(demo->depth.image);
    err = demo->device.createImageView(&view, nullptr, &demo->depth.view);
    assert(err == vk::Result::eSuccess);
}

static void
demo_prepare_texture_image(Demo *demo, const uint32_t *tex_colors,
                           struct texture_object *tex_obj, vk::ImageTiling tiling,
                           vk::ImageUsageFlags usage,
                           vk::MemoryPropertyFlags required_props) {
    const vk::Format tex_format = vk::Format::eB8G8R8A8Unorm;
    const int32_t tex_width = 2;
    const int32_t tex_height = 2;
    vk::Result U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;

    tex_obj->tex_width = tex_width;
    tex_obj->tex_height = tex_height;

    const vk::ImageCreateInfo image_create_info = vk::ImageCreateInfo()
        .imageType(vk::ImageType::e2D)
        .format(tex_format)
        .extent(vk::Extent3D(tex_width, tex_height, 1))
        .mipLevels(1)
        .arrayLayers(1)
        .samples(vk::SampleCountFlagBits::e1)
        .tiling(tiling)
        .usage(usage)
        .initialLayout(vk::ImageLayout::ePreinitialized);

    vk::MemoryAllocateInfo mem_alloc;
    vk::MemoryRequirements mem_reqs;

    err = demo->device.createImage(&image_create_info, nullptr, &tex_obj->image);
    assert(err == vk::Result::eSuccess);

    demo->device.getImageMemoryRequirements(tex_obj->image, &mem_reqs);

    mem_alloc.allocationSize(mem_reqs.size());
    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits(),
                                       required_props, mem_alloc);
    assert(pass);

    /* allocate memory */
    err = demo->device.allocateMemory(&mem_alloc, nullptr, &tex_obj->mem);
    assert(err == vk::Result::eSuccess);

    /* bind memory */
    err = demo->device.bindImageMemory(tex_obj->image, tex_obj->mem, 0);
    assert(err == vk::Result::eSuccess);

    if (required_props & vk::MemoryPropertyFlagBits::eHostVisible) {
        const vk::ImageSubresource subres =
          vk::ImageSubresource().aspectMask(vk::ImageAspectFlagBits::eColor);
        vk::SubresourceLayout layout;
        void *data;

        demo->device.getImageSubresourceLayout(tex_obj->image, &subres, &layout);

        err = demo->device.mapMemory(tex_obj->mem, 0, mem_alloc.allocationSize(),
                                     vk::MemoryMapFlags(), &data);
        assert(err == vk::Result::eSuccess);

        for (int32_t y = 0; y < tex_height; y++) {
            uint32_t *row = (uint32_t *)((char *)data + layout.rowPitch() * y);
            for (int32_t x = 0; x < tex_width; x++)
                row[x] = tex_colors[(x & 1) ^ (y & 1)];
        }

        demo->device.unmapMemory(tex_obj->mem);
    }

    tex_obj->imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    demo_set_image_layout(demo, tex_obj->image, vk::ImageAspectFlagBits::eColor,
                          vk::ImageLayout::eUndefined, tex_obj->imageLayout);
    /* setting the image layout does not reference the actual memory so no need
     * to add a mem ref */
}

static void demo_destroy_texture_image(Demo *demo,
                                       struct texture_object *tex_obj) {
    /* clean up staging resources */
    demo->device.destroyImage(tex_obj->image, nullptr);
    demo->device.freeMemory(tex_obj->mem, nullptr);
}

static void demo_prepare_textures(Demo *demo) {
    const vk::Format tex_format = vk::Format::eB8G8R8A8Unorm;
    vk::FormatProperties props;
    const uint32_t tex_colors[DEMO_TEXTURE_COUNT][2] = {
        {0xffff0000, 0xff00ff00},
    };
    vk::Result U_ASSERT_ONLY err;

    demo->gpu.getFormatProperties(tex_format, &props);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if ((props.linearTilingFeatures() &
             vk::FormatFeatureFlagBits::eSampledImage) &&
            !demo->use_staging_buffer) {
            /* Device can texture using linear textures */
            demo_prepare_texture_image(demo, tex_colors[i], &demo->textures[i],
                                       vk::ImageTiling::eLinear,
                                       vk::ImageUsageFlagBits::eSampled,
                                       vk::MemoryPropertyFlagBits::eHostVisible);
        } else if (props.optimalTilingFeatures() &
                   vk::FormatFeatureFlagBits::eSampledImage) {
            /* Must use staging buffer to copy linear texture to optimized */
            struct texture_object staging_texture;

            demo_prepare_texture_image(demo, tex_colors[i], &staging_texture,
                                       vk::ImageTiling::eLinear,
                                       vk::ImageUsageFlagBits::eTransferSrc,
                                       vk::MemoryPropertyFlagBits::eHostVisible);

            demo_prepare_texture_image(
                demo, tex_colors[i], &demo->textures[i],
                vk::ImageTiling::eOptimal,
                (vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled),
                vk::MemoryPropertyFlagBits::eDeviceLocal);

            demo_set_image_layout(demo, staging_texture.image,
                                  vk::ImageAspectFlagBits::eColor,
                                  staging_texture.imageLayout,
                                  vk::ImageLayout::eTransferSrcOptimal);

            demo_set_image_layout(demo, demo->textures[i].image,
                                  vk::ImageAspectFlagBits::eColor,
                                  demo->textures[i].imageLayout,
                                  vk::ImageLayout::eTransferDstOptimal);

            vk::ImageCopy copy_region = vk::ImageCopy()
                .srcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .dstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .extent(vk::Extent3D(staging_texture.tex_width,
                                     staging_texture.tex_height, 1));
            demo->setup_cmd.copyImage(staging_texture.image,
                vk::ImageLayout::eTransferSrcOptimal, demo->textures[i].image,
                vk::ImageLayout::eTransferDstOptimal, 1, &copy_region);

            demo_set_image_layout(demo, demo->textures[i].image,
                                  vk::ImageAspectFlagBits::eColor,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  demo->textures[i].imageLayout);

            demo_flush_init_cmd(demo);

            demo_destroy_texture_image(demo, &staging_texture);
        } else {
            /* Can't support vk::Format::eB8G8R8A8Unorm !? */
            assert(!"No support for B8G8R8A8_UNORM as texture image format");
        }

        const vk::SamplerCreateInfo sampler = vk::SamplerCreateInfo()
            .magFilter(vk::Filter::eNearest)
            .minFilter(vk::Filter::eNearest)
            .mipmapMode(vk::SamplerMipmapMode::eNearest)
            .addressModeU(vk::SamplerAddressMode::eRepeat)
            .addressModeV(vk::SamplerAddressMode::eRepeat)
            .addressModeW(vk::SamplerAddressMode::eRepeat)
            .mipLodBias(0.0f)
            .anisotropyEnable(VK_FALSE)
            .maxAnisotropy(1)
            .compareOp(vk::CompareOp::eNever)
            .minLod(0.0f)
            .maxLod(0.0f)
            .borderColor(vk::BorderColor::eFloatOpaqueWhite)
            .unnormalizedCoordinates(VK_FALSE);

        vk::ImageViewCreateInfo view = vk::ImageViewCreateInfo()
            .viewType(vk::ImageViewType::e2D)
            .format(tex_format)
            .subresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        /* create sampler */
        err = demo->device.createSampler(&sampler, nullptr,
                                         &demo->textures[i].sampler);
        assert(err == vk::Result::eSuccess);

        /* create image view */
        view.image(demo->textures[i].image);
        err = demo->device.createImageView(&view, nullptr, &demo->textures[i].view);
        assert(err == vk::Result::eSuccess);
    }
}

static void demo_prepare_vertices(Demo *demo) {
    // clang-format off
    const float vb[3][5] = {
        /*      position             texcoord */
        { -1.0f, -1.0f,  0.25f,     0.0f, 0.0f },
        {  1.0f, -1.0f,  0.25f,     1.0f, 0.0f },
        {  0.0f,  1.0f,  1.0f,      0.5f, 1.0f },
    };
    // clang-format on
    const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
        .size(sizeof(vb))
        .usage(vk::BufferUsageFlagBits::eVertexBuffer);

    vk::MemoryAllocateInfo mem_alloc;
    vk::MemoryRequirements mem_reqs;
    vk::Result U_ASSERT_ONLY err;
    bool U_ASSERT_ONLY pass;
    void *data;

    err = demo->device.createBuffer(&buf_info, nullptr, &demo->vertices.buf);
    assert(err == vk::Result::eSuccess);

    demo->device.getBufferMemoryRequirements(demo->vertices.buf, &mem_reqs);
    assert(err == vk::Result::eSuccess);

    mem_alloc.allocationSize(mem_reqs.size());
    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits(),
                                       vk::MemoryPropertyFlagBits::eHostVisible,
                                       mem_alloc);
    assert(pass);

    err = demo->device.allocateMemory(&mem_alloc, nullptr, &demo->vertices.mem);
    assert(err == vk::Result::eSuccess);

    err = demo->device.mapMemory(demo->vertices.mem, 0, mem_alloc.allocationSize(),
                                 vk::MemoryMapFlags{}, &data);
    assert(err == vk::Result::eSuccess);

    memcpy(data, vb, sizeof(vb));

    demo->device.unmapMemory(demo->vertices.mem);

    err = demo->device.bindBufferMemory(demo->vertices.buf, demo->vertices.mem, 0);
    assert(err == vk::Result::eSuccess);

    demo->vertices.vi.vertexBindingDescriptionCount(1);
    demo->vertices.vi.pVertexBindingDescriptions(demo->vertices.vi_bindings);
    demo->vertices.vi.vertexAttributeDescriptionCount(2);
    demo->vertices.vi.pVertexAttributeDescriptions(demo->vertices.vi_attrs);

    demo->vertices.vi_bindings[0].binding(VERTEX_BUFFER_BIND_ID);
    demo->vertices.vi_bindings[0].stride(sizeof(vb[0]));
    demo->vertices.vi_bindings[0].inputRate(vk::VertexInputRate::eVertex);

    demo->vertices.vi_attrs[0].binding(VERTEX_BUFFER_BIND_ID);
    demo->vertices.vi_attrs[0].location(0);
    demo->vertices.vi_attrs[0].format(vk::Format::eR32G32B32Sfloat);
    demo->vertices.vi_attrs[0].offset(0);

    demo->vertices.vi_attrs[1].binding(VERTEX_BUFFER_BIND_ID);
    demo->vertices.vi_attrs[1].location(1);
    demo->vertices.vi_attrs[1].format(vk::Format::eR32G32Sfloat);
    demo->vertices.vi_attrs[1].offset(sizeof(float) * 3);
}

static void demo_prepare_descriptor_layout(Demo *demo) {
    const vk::DescriptorSetLayoutBinding layout_binding =
      vk::DescriptorSetLayoutBinding()
        .binding(0)
        .descriptorType(vk::DescriptorType::eCombinedImageSampler)
        .descriptorCount(DEMO_TEXTURE_COUNT)
        .stageFlags(vk::ShaderStageFlagBits::eFragment);

    const vk::DescriptorSetLayoutCreateInfo descriptor_layout =
      vk::DescriptorSetLayoutCreateInfo()
        .bindingCount(1)
        .pBindings(&layout_binding);

    vk::Result U_ASSERT_ONLY err;

    err = demo->device.createDescriptorSetLayout(&descriptor_layout, nullptr,
                                                 &demo->desc_layout);
    assert(err == vk::Result::eSuccess);

    const vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
      vk::PipelineLayoutCreateInfo()
        .setLayoutCount(1)
        .pSetLayouts(&demo->desc_layout);

    err = demo->device.createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr,
                                            &demo->pipeline_layout);
    assert(err == vk::Result::eSuccess);
}

static void demo_prepare_render_pass(Demo *demo) {
    const vk::AttachmentDescription attachments[2] = {
      vk::AttachmentDescription()
        .format(demo->format)
        .samples(vk::SampleCountFlagBits::e1)
        .loadOp(vk::AttachmentLoadOp::eClear)
        .storeOp(vk::AttachmentStoreOp::eStore)
        .stencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .stencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .initialLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .finalLayout(vk::ImageLayout::eColorAttachmentOptimal),
      vk::AttachmentDescription()
        .format(demo->depth.format)
        .samples(vk::SampleCountFlagBits::e1)
        .loadOp(vk::AttachmentLoadOp::eClear)
        .storeOp(vk::AttachmentStoreOp::eDontCare)
        .stencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .stencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .initialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .finalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
    };
    const vk::AttachmentReference color_reference = vk::AttachmentReference()
        .attachment(0)
        .layout(vk::ImageLayout::eColorAttachmentOptimal);
    const vk::AttachmentReference depth_reference = vk::AttachmentReference()
        .attachment(1)
        .layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    const vk::SubpassDescription subpass = vk::SubpassDescription()
        .pipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .colorAttachmentCount(1)
        .pColorAttachments(&color_reference)
        .pDepthStencilAttachment(&depth_reference);
    const vk::RenderPassCreateInfo rp_info = vk::RenderPassCreateInfo()
        .attachmentCount(2)
        .pAttachments(attachments)
        .subpassCount(1)
        .pSubpasses(&subpass);
    vk::Result U_ASSERT_ONLY err;

    err = demo->device.createRenderPass(&rp_info, nullptr, &demo->render_pass);
    assert(err == vk::Result::eSuccess);
}

static void demo_prepare_descriptor_pool(Demo *demo) {
    const vk::DescriptorPoolSize type_count = vk::DescriptorPoolSize()
        .type(vk::DescriptorType::eCombinedImageSampler)
        .descriptorCount(DEMO_TEXTURE_COUNT);

    const vk::DescriptorPoolCreateInfo descriptor_pool =
      vk::DescriptorPoolCreateInfo()
        .maxSets(1)
        .poolSizeCount(1)
        .pPoolSizes(&type_count);

    vk::Result U_ASSERT_ONLY err;
    err = demo->device.createDescriptorPool(&descriptor_pool, nullptr,
                                            &demo->desc_pool);
    assert(err == vk::Result::eSuccess);
}

static void demo_prepare_descriptor_set(Demo *demo) {
    vk::DescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
    vk::WriteDescriptorSet write;
    vk::Result U_ASSERT_ONLY err;

    vk::DescriptorSetAllocateInfo alloc_info =
      vk::DescriptorSetAllocateInfo()
        .descriptorPool(demo->desc_pool)
        .descriptorSetCount(1)
        .pSetLayouts(&demo->desc_layout);
    err = demo->device.allocateDescriptorSets(&alloc_info, &demo->desc_set);
    assert(err == vk::Result::eSuccess);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        tex_descs[i].sampler(demo->textures[i].sampler);
        tex_descs[i].imageView(demo->textures[i].view);
        tex_descs[i].imageLayout(vk::ImageLayout::eGeneral);
    }

    write.dstSet(demo->desc_set);
    write.descriptorCount(DEMO_TEXTURE_COUNT);
    write.descriptorType(vk::DescriptorType::eCombinedImageSampler);
    write.pImageInfo(tex_descs);

    demo->device.updateDescriptorSets(1, &write, 0, nullptr);
}

static void demo_prepare_framebuffers(Demo *demo) {
    vk::ImageView attachments[2];
    attachments[1] = demo->depth.view;

    const vk::FramebufferCreateInfo fb_info =
      vk::FramebufferCreateInfo()
        .renderPass(demo->render_pass)
        .attachmentCount(2)
        .pAttachments(attachments)
        .width(demo->width)
        .height(demo->height)
        .layers(1);
    vk::Result U_ASSERT_ONLY err;

    demo->framebuffers = new vk::Framebuffer[demo->swapchainImageCount];

    for (uint32_t i = 0; i < demo->swapchainImageCount; i++) {
        attachments[0] = demo->buffers[i].view;
        err = demo->device.createFramebuffer(&fb_info, nullptr,
                                             &demo->framebuffers[i]);
        assert(err == vk::Result::eSuccess);
    }
}

static void demo_prepare(Demo *demo) {
    vk::Result U_ASSERT_ONLY err;

    const vk::CommandPoolCreateInfo cmd_pool_info =
      vk::CommandPoolCreateInfo()
        .queueFamilyIndex(demo->graphics_queue_node_index)
        .flags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    err = demo->device.createCommandPool(&cmd_pool_info, nullptr,
                                         &demo->cmd_pool);
    assert(err == vk::Result::eSuccess);

    const vk::CommandBufferAllocateInfo cmd = vk::CommandBufferAllocateInfo()
      .commandPool(demo->cmd_pool)
      .level(vk::CommandBufferLevel::ePrimary)
      .commandBufferCount(1);

    err = demo->device.allocateCommandBuffers(&cmd, &demo->draw_cmd);
    assert(err == vk::Result::eSuccess);

    demo_prepare_buffers(demo);
    demo_prepare_depth(demo);
    demo_prepare_textures(demo);
    demo_prepare_vertices(demo);
    demo_prepare_descriptor_layout(demo);
    demo_prepare_render_pass(demo);
    demo->pipeline = Initialize::PreparePipeline(demo->device, demo->vertices.vi,
                                                demo->pipeline_layout, demo->render_pass);

    demo_prepare_descriptor_pool(demo);
    demo_prepare_descriptor_set(demo);

    demo_prepare_framebuffers(demo);
}

static void demo_error_callback(int error, const char* description) {
    printf("GLFW error: %s\n", description);
    fflush(stdout);
}

static void demo_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void demo_refresh_callback(GLFWwindow* window) {
    Demo* demo = reinterpret_cast<Demo*>(glfwGetWindowUserPointer(window));
    demo_draw(demo);
}

static void demo_resize_callback(GLFWwindow* window, int width, int height) {
    Demo* demo = reinterpret_cast<Demo*>(glfwGetWindowUserPointer(window));
    demo->width = width;
    demo->height = height;
    demo_resize(demo);
}

static void demo_run(Demo *demo) {
    while (!glfwWindowShouldClose(demo->window)) {
        glfwPollEvents();

        demo_draw(demo);

        if (demo->depthStencil > 0.99f)
            demo->depthIncrement = -0.001f;
        if (demo->depthStencil < 0.8f)
            demo->depthIncrement = 0.001f;

        demo->depthStencil += demo->depthIncrement;

        // Wait for work to finish before updating MVP.
        demo->device.waitIdle();
    }
}

static void demo_create_window(Demo *demo) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    demo->window = glfwCreateWindow(demo->width,
                                    demo->height,
                                    APP_LONG_NAME,
                                    nullptr,
                                    nullptr);
    if (!demo->window) {
        // It didn't work, so try to give a useful error:
        printf("Cannot create a window in which to draw!\n");
        fflush(stdout);
        exit(1);
    }

    glfwSetWindowUserPointer(demo->window, demo);
    glfwSetWindowRefreshCallback(demo->window, demo_refresh_callback);
    glfwSetFramebufferSizeCallback(demo->window, demo_resize_callback);
    glfwSetKeyCallback(demo->window, demo_key_callback);
}

/*
 * Return 1 (true) if all layer names specified in check_names
 * can be found in given layer properties.
 */
static vk::Bool32 demo_check_layers(uint32_t check_count,
                                    const char **check_names,
                                    uint32_t layer_count,
                                    vk::LayerProperties *layers) {
    for (uint32_t i = 0; i < check_count; i++) {
        vk::Bool32 found = 0;
        for (uint32_t j = 0; j < layer_count; j++) {
            if (!strcmp(check_names[i], layers[j].layerName())) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Cannot find layer: %s\n", check_names[i]);
            return 0;
        }
    }
    return 1;
}

static void demo_init_vk(Demo *demo) {
    vk::Result err;
    uint32_t required_extension_count;
    const char** required_extensions;
    uint32_t instance_extension_count = 0;
    uint32_t instance_layer_count = 0;
    uint32_t device_validation_layer_count = 0;
    demo->enabled_extension_count = 0;
    demo->enabled_layer_count = 0;

    Shader::InitializeGlslang();

    const char *instance_validation_layers[] = {
        // "VK_LAYER_GOOGLE_threading",
        "VK_LAYER_LUNARG_param_checker",
        "VK_LAYER_LUNARG_device_limits",
        "VK_LAYER_LUNARG_object_tracker",
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_mem_tracker",
        "VK_LAYER_LUNARG_draw_state",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects"
    };

    device_validation_layer_count = ARRAY_SIZE(instance_validation_layers);
    for (int i = 0; i < device_validation_layer_count; ++i) {
      demo->device_validation_layers[i] = instance_validation_layers[i];
    }
    // demo->device_validation_layers[0] = "VK_LAYER_LUNARG_mem_tracker";
    // demo->device_validation_layers[1] = "VK_LAYER_GOOGLE_unique_objects";
    // device_validation_layer_count = 2;

    /* Look for validation layers */
    vk::Bool32 validation_found = 0;
    err = vk::enumerateInstanceLayerProperties(&instance_layer_count, nullptr);
    assert(err == vk::Result::eSuccess);

    if (instance_layer_count > 0) {
        vk::LayerProperties *instance_layers =
            new vk::LayerProperties[instance_layer_count];
        err = vk::enumerateInstanceLayerProperties(&instance_layer_count,
                                                   instance_layers);
        assert(err == vk::Result::eSuccess);

        if (demo->validate) {
            validation_found = demo_check_layers(
                ARRAY_SIZE(instance_validation_layers),
                instance_validation_layers, instance_layer_count,
                instance_layers);
            demo->enabled_layer_count = ARRAY_SIZE(instance_validation_layers);
        }

        delete[] instance_layers;
    }

    if (demo->validate && !validation_found) {
        throw std::runtime_error("vk::enumerateInstanceLayerProperties failed to find"
                 "required validation layer.\n\n"
                 "Please look at the Getting Started guide for additional information.");
    }

    /* Look for instance extensions */
    required_extensions = glfwGetRequiredInstanceExtensions((unsigned*) &required_extension_count);
    if (!required_extensions) {
        throw std::runtime_error("glfwGetRequiredInstanceExtensions failed to find the "
                 "platform surface extensions.\n\nDo you have a compatible "
                 "Vulkan installable client driver (ICD) installed?\nPlease "
                 "look at the Getting Started guide for additional information.");
    }

    for (uint32_t i = 0; i < required_extension_count; i++) {
        demo->extension_names[demo->enabled_extension_count++] = required_extensions[i];
        assert(demo->enabled_extension_count < 64);
    }

    err = vk::enumerateInstanceExtensionProperties(
        nullptr, &instance_extension_count, nullptr);
    assert(err == vk::Result::eSuccess);

    if (instance_extension_count > 0) {
        vk::ExtensionProperties *instance_extensions =
            new vk::ExtensionProperties[instance_extension_count];
        err = vk::enumerateInstanceExtensionProperties(
            nullptr, &instance_extension_count, instance_extensions);
        assert(err == vk::Result::eSuccess);
        for (uint32_t i = 0; i < instance_extension_count; i++) {
            if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                        instance_extensions[i].extensionName())) {
                if (demo->validate) {
                    demo->extension_names[demo->enabled_extension_count++] =
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
                }
            }
            assert(demo->enabled_extension_count < 64);
        }

        delete[] instance_extensions;
    }

    const vk::ApplicationInfo app = vk::ApplicationInfo()
        .pApplicationName(APP_SHORT_NAME)
        .applicationVersion(0)
        .pEngineName(APP_SHORT_NAME)
        .engineVersion(0)
        .apiVersion(VK_MAKE_VERSION(1, 0, 4));

    vk::InstanceCreateInfo inst_info = vk::InstanceCreateInfo()
        .pApplicationInfo(&app)
        .enabledLayerCount(demo->enabled_layer_count)
        .ppEnabledLayerNames((const char *const *)instance_validation_layers)
        .enabledExtensionCount(demo->enabled_extension_count)
        .ppEnabledExtensionNames((const char *const *)demo->extension_names);

    uint32_t gpu_count;

    err = vk::createInstance(&inst_info, nullptr, &demo->inst);
    if (err == vk::Result::eErrorIncompatibleDriver) {
        throw std::runtime_error("Cannot find a compatible Vulkan installable client driver "
                 "(ICD).\n\nPlease look at the Getting Started guide for "
                 "additional information.");
    } else if (err == vk::Result::eErrorExtensionNotPresent) {
        throw std::runtime_error("Cannot find a specified extension library"
                 ".\nMake sure your layers path is set appropriately");
    } else if (err != vk::Result::eSuccess) {
        throw std::runtime_error("vk::createInstance failed.\n\nDo you have a compatible Vulkan "
                 "installable client driver (ICD) installed?\nPlease look at "
                 "the Getting Started guide for additional information.");
    }

    /* Make initial call to query gpu_count, then second call for gpu info*/
    err = demo->inst.enumeratePhysicalDevices(&gpu_count, nullptr);
    assert(err == vk::Result::eSuccess && gpu_count > 0);

    if (gpu_count > 0) {
        vk::PhysicalDevice *physical_devices =
            new vk::PhysicalDevice[gpu_count];
        err = demo->inst.enumeratePhysicalDevices(&gpu_count, physical_devices);
        assert(err == vk::Result::eSuccess);
        /* For tri demo we just grab the first physical device */
        demo->gpu = physical_devices[0];
        delete[] physical_devices;
    } else {
        throw std::runtime_error("vk::enumeratePhysicalDevices reported zero accessible devices."
                 "\n\nDo you have a compatible Vulkan installable client"
                 " driver (ICD) installed?\nPlease look at the Getting Started"
                 " guide for additional information.");
    }

    /* Look for validation layers */
    validation_found = 0;
    demo->enabled_layer_count = 0;
    uint32_t device_layer_count = 0;
    err = demo->gpu.enumerateDeviceLayerProperties(&device_layer_count, nullptr);
    assert(err == vk::Result::eSuccess);

    if (device_layer_count > 0) {
        vk::LayerProperties *device_layers =
            new vk::LayerProperties[device_layer_count];
        err = demo->gpu.enumerateDeviceLayerProperties(&device_layer_count,
                                                       device_layers);
        assert(err == vk::Result::eSuccess);

        if (demo->validate) {
            validation_found = demo_check_layers(device_validation_layer_count,
                                                 demo->device_validation_layers,
                                                 device_layer_count,
                                                 device_layers);
            demo->enabled_layer_count = device_validation_layer_count;
        }

        delete[] device_layers;
    }

    if (demo->validate && !validation_found) {
        throw std::runtime_error("vk::enumerateDeviceLayerProperties failed to find "
                 "a required validation layer.\n\n"
                 "Please look at the Getting Started guide for additional information.");
    }

    /* Look for device extensions */
    uint32_t device_extension_count = 0;
    vk::Bool32 swapchainExtFound = 0;
    demo->enabled_extension_count = 0;

    err = demo->gpu.enumerateDeviceExtensionProperties(
        nullptr, &device_extension_count, nullptr);
    assert(err == vk::Result::eSuccess);

    if (device_extension_count > 0) {
        vk::ExtensionProperties *device_extensions =
            new vk::ExtensionProperties[device_extension_count];
        err = demo->gpu.enumerateDeviceExtensionProperties(
            nullptr, &device_extension_count, device_extensions);
        assert(err == vk::Result::eSuccess);

        for (uint32_t i = 0; i < device_extension_count; i++) {
            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                        device_extensions[i].extensionName())) {
                swapchainExtFound = 1;
                demo->extension_names[demo->enabled_extension_count++] =
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            }
            assert(demo->enabled_extension_count < 64);
        }

        delete[] device_extensions;
    }

    if (!swapchainExtFound) {
        throw std::runtime_error("vk::enumerateDeviceExtensionProperties failed to find "
                 "the " VK_KHR_SWAPCHAIN_EXTENSION_NAME
                 " extension.\n\nDo you have a compatible "
                 "Vulkan installable client driver (ICD) installed?\nPlease "
                 "look at the Getting Started guide for additional information.");
    }

    if (demo->validate) {
        demo->CreateDebugReportCallback =
            (PFN_vkCreateDebugReportCallbackEXT) demo->inst.getProcAddr(
                "vkCreateDebugReportCallbackEXT");
        if (!demo->CreateDebugReportCallback) {
            throw std::runtime_error("GetProcAddr: Unable to find vkCreateDebugReportCallbackEXT");
        }
        vk::DebugReportCallbackCreateInfoEXT dbgCreateInfo{
          vk::DebugReportFlagBitsEXT::eError |
          vk::DebugReportFlagBitsEXT::eWarning,
          dbgFunc, nullptr};
        VkDebugReportCallbackCreateInfoEXT vkDbgCreateInfo = dbgCreateInfo;

        VkResult vkErr = demo->CreateDebugReportCallback(demo->inst, &vkDbgCreateInfo,
                                              nullptr, &demo->msg_callback);
        switch (vkErr) {
          case VK_SUCCESS:
              break;
          case VK_ERROR_OUT_OF_HOST_MEMORY:
              throw std::runtime_error("CreateDebugReportCallback: out of host memory");
              break;
          default:
              throw std::runtime_error("CreateDebugReportCallback: unknown failure");
              break;
        }
    }

    // Having these GIPA queries of device extension entry points both
    // BEFORE and AFTER vk::createDevice is a good test for the loader
    GET_INSTANCE_PROC_ADDR(demo->inst, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, GetPhysicalDeviceSurfaceFormatsKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, GetPhysicalDeviceSurfacePresentModesKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, GetPhysicalDeviceSurfaceSupportKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, CreateSwapchainKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, DestroySwapchainKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, GetSwapchainImagesKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, AcquireNextImageKHR);
    GET_INSTANCE_PROC_ADDR(demo->inst, QueuePresentKHR);

    demo->gpu.getProperties(&demo->gpu_props);

    // Query with nullptr data to get count
    demo->gpu.getQueueFamilyProperties(&demo->queue_count, nullptr);

    demo->queue_props = new vk::QueueFamilyProperties[demo->queue_count];
    demo->gpu.getQueueFamilyProperties(&demo->queue_count, demo->queue_props);
    assert(demo->queue_count >= 1);

    // Graphics queue and MemMgr queue can be separate.
    // TODO: Add support for separate queues, including synchronization,
    //       and appropriate tracking for QueueSubmit
}

static void demo_init_device(Demo *demo) {
    vk::Result U_ASSERT_ONLY err;

    float queue_priorities[1] = {0.0};
    const vk::DeviceQueueCreateInfo queue = vk::DeviceQueueCreateInfo()
        .queueFamilyIndex(demo->graphics_queue_node_index)
        .queueCount(1)
        .pQueuePriorities(queue_priorities);

    vk::DeviceCreateInfo device = vk::DeviceCreateInfo()
        .queueCreateInfoCount(1)
        .pQueueCreateInfos(&queue)
        .enabledLayerCount(demo->enabled_layer_count)
        .ppEnabledLayerNames((const char *const *)((demo->validate)
                                      ? demo->device_validation_layers
                                      : nullptr))
        .enabledExtensionCount(demo->enabled_extension_count)
        .ppEnabledExtensionNames((const char *const *)demo->extension_names);

    err = demo->gpu.createDevice(&device, nullptr, &demo->device);
    assert(err == vk::Result::eSuccess);

    GET_DEVICE_PROC_ADDR(demo->device, CreateSwapchainKHR);
    GET_DEVICE_PROC_ADDR(demo->device, DestroySwapchainKHR);
    GET_DEVICE_PROC_ADDR(demo->device, GetSwapchainImagesKHR);
    GET_DEVICE_PROC_ADDR(demo->device, AcquireNextImageKHR);
    GET_DEVICE_PROC_ADDR(demo->device, QueuePresentKHR);
}

static void demo_init_vk_swapchain(Demo *demo) {
    vk::Result U_ASSERT_ONLY err;

    // Create a WSI surface for the window:
    glfwCreateWindowSurface(demo->inst, demo->window, nullptr, &demo->surface);

    // Iterate over each queue to learn whether it supports presenting:
    vk::Bool32 *supportsPresent = new vk::Bool32[demo->queue_count];
    for (uint32_t i = 0; i < demo->queue_count; i++) {
        demo->fpGetPhysicalDeviceSurfaceSupportKHR(demo->gpu, i, demo->surface,
                                                   &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < demo->queue_count; i++) {
        if ((demo->queue_props[i].queueFlags() & vk::QueueFlagBits::eGraphics) != 0) {
            if (graphicsQueueNodeIndex == UINT32_MAX) {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE) {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for (uint32_t i = 0; i < demo->queue_count; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    delete[] supportsPresent;

    // Generate error if could not find both a graphics and a present queue
    if (graphicsQueueNodeIndex == UINT32_MAX ||
        presentQueueNodeIndex == UINT32_MAX) {
        throw std::runtime_error("Could not find a graphics and a present queue");
    }

    // TODO: Add support for separate queues, including presentation,
    //       synchronization, and appropriate tracking for QueueSubmit.
    // NOTE: While it is possible for an application to use a separate graphics
    //       and a present queues, this demo program assumes it is only using
    //       one:
    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
        throw std::runtime_error("Could not find a common graphics and a present queue");
    }

    demo->graphics_queue_node_index = graphicsQueueNodeIndex;

    demo_init_device(demo);

    demo->device.getQueue(demo->graphics_queue_node_index, 0, &demo->queue);

    // Get the list of vk::Format's that are supported:
    uint32_t formatCount;
    VkResult vkerr =
      demo->fpGetPhysicalDeviceSurfaceFormatsKHR(demo->gpu, demo->surface,
                                                 &formatCount, nullptr);
    assert(vkerr == VK_SUCCESS);
    VkSurfaceFormatKHR *surfFormats = new VkSurfaceFormatKHR[formatCount];
    vkerr = demo->fpGetPhysicalDeviceSurfaceFormatsKHR(demo->gpu, demo->surface,
                                                       &formatCount, surfFormats);
    assert(vkerr == VK_SUCCESS);
    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        demo->format = vk::Format::eB8G8R8A8Unorm;
    } else {
        assert(formatCount >= 1);
        demo->format = static_cast<vk::Format>(surfFormats[0].format);
    }
    demo->color_space = static_cast<vk::ColorSpaceKHR>(surfFormats[0].colorSpace);

    // Get Memory information and properties
    demo->gpu.getMemoryProperties(&demo->memory_properties);
}

static void demo_init_connection(Demo *demo) {
    glfwSetErrorCallback(demo_error_callback);

    if (!glfwInit()) {
        printf("Cannot initialize GLFW.\nExiting ...\n");
        fflush(stdout);
        exit(1);
    }

    if (!glfwVulkanSupported()) {
        printf("Cannot find a compatible Vulkan installable client driver "
               "(ICD).\nExiting ...\n");
        fflush(stdout);
        exit(1);
    }
}

static void demo_init(Demo *demo, const int argc, const char *argv[]) {
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--use_staging", strlen("--use_staging")) == 0)
            demo->use_staging_buffer = true;
    }

    demo_init_connection(demo);
    demo_init_vk(demo);

    demo->width = 300;
    demo->height = 300;
    demo->depthStencil = 1.0;
    demo->depthIncrement = -0.01f;
}

static void demo_cleanup(Demo *demo) {
    for (uint32_t i = 0; i < demo->swapchainImageCount; i++) {
        demo->device.destroyFramebuffer(demo->framebuffers[i], nullptr);
    }
    delete[] demo->framebuffers;
    demo->device.destroyDescriptorPool(demo->desc_pool, nullptr);

    if (demo->setup_cmd) {
        demo->device.freeCommandBuffers(demo->cmd_pool, 1, &demo->setup_cmd);
    }
    demo->device.freeCommandBuffers(demo->cmd_pool, 1, &demo->draw_cmd);
    demo->device.destroyCommandPool(demo->cmd_pool, nullptr);

    demo->device.destroyPipeline(demo->pipeline, nullptr);
    demo->device.destroyRenderPass(demo->render_pass, nullptr);
    demo->device.destroyPipelineLayout(demo->pipeline_layout, nullptr);
    demo->device.destroyDescriptorSetLayout(demo->desc_layout, nullptr);

    demo->device.destroyBuffer(demo->vertices.buf, nullptr);
    demo->device.freeMemory(demo->vertices.mem, nullptr);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        demo->device.destroyImageView(demo->textures[i].view, nullptr);
        demo->device.destroyImage(demo->textures[i].image, nullptr);
        demo->device.freeMemory(demo->textures[i].mem, nullptr);
        demo->device.destroySampler(demo->textures[i].sampler, nullptr);
    }

    for (uint32_t i = 0; i < demo->swapchainImageCount; i++) {
        demo->device.destroyImageView(demo->buffers[i].view, nullptr);
    }

    demo->device.destroyImageView(demo->depth.view, nullptr);
    demo->device.destroyImage(demo->depth.image, nullptr);
    demo->device.freeMemory(demo->depth.mem, nullptr);

    demo->fpDestroySwapchainKHR(demo->device, demo->swapchain, nullptr);
    delete[] demo->buffers;

    demo->device.destroy(nullptr);
    demo->inst.destroySurfaceKHR(demo->surface, nullptr);

    delete[] demo->queue_props;

    Shader::FinalizeGlslang();

    glfwDestroyWindow(demo->window);
    glfwTerminate();
}

static void demo_resize(Demo *demo) {
    for (uint32_t i = 0; i < demo->swapchainImageCount; i++) {
        demo->device.destroyFramebuffer(demo->framebuffers[i], nullptr);
    }
    delete[] demo->framebuffers;
    demo->device.destroyDescriptorPool(demo->desc_pool, nullptr);

    if (demo->setup_cmd) {
        demo->device.freeCommandBuffers(demo->cmd_pool, 1, &demo->setup_cmd);
    }
    demo->device.freeCommandBuffers(demo->cmd_pool, 1, &demo->draw_cmd);
    demo->device.destroyCommandPool(demo->cmd_pool, nullptr);

    demo->device.destroyPipeline(demo->pipeline, nullptr);
    demo->device.destroyRenderPass(demo->render_pass, nullptr);
    demo->device.destroyPipelineLayout(demo->pipeline_layout, nullptr);
    demo->device.destroyDescriptorSetLayout(demo->desc_layout, nullptr);

    demo->device.destroyBuffer(demo->vertices.buf, nullptr);
    demo->device.freeMemory(demo->vertices.mem, nullptr);

    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        demo->device.destroyImageView(demo->textures[i].view, nullptr);
        demo->device.destroyImage(demo->textures[i].image, nullptr);
        demo->device.freeMemory(demo->textures[i].mem, nullptr);
        demo->device.destroySampler(demo->textures[i].sampler, nullptr);
    }

    for (uint32_t i = 0; i < demo->swapchainImageCount; i++) {
        demo->device.destroyImageView(demo->buffers[i].view, nullptr);
    }

    demo->device.destroyImageView(demo->depth.view, nullptr);
    demo->device.destroyImage(demo->depth.image, nullptr);
    demo->device.freeMemory(demo->depth.mem, nullptr);

    delete[] demo->buffers;

    // Second, re-perform the demo_prepare() function, which will re-create the
    // swapchain:
    demo_prepare(demo);
}

int main(const int argc, const char *argv[]) {
    Demo demo;

    demo_init(&demo, argc, argv);
    demo_create_window(&demo);
    demo_init_vk_swapchain(&demo);

    demo_prepare(&demo);
    demo_run(&demo);

    demo_cleanup(&demo);

    return 0;
}

