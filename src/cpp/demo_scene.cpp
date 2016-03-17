#include "demo_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cassert>
#include <iostream>
#include <vector>
#include <memory>

#include <vulkan/vk_cpp.h>
#include <GLFW/glfw3.h>

#include "common/error_checking.hpp"
#include "initialize/debug_callback.hpp"
#include "initialize/create_physical_device.hpp"
#include "initialize/create_pipeline.hpp"
#include "initialize/prepare_swapchain.hpp"

#define VERTEX_BUFFER_BIND_ID 0

#if defined(NDEBUG) && defined(__GNUC__)
  #define U_ASSERT_ONLY __attribute__((unused))
#else
  #define U_ASSERT_ONLY
#endif

// Forward declaration:
static void demo_resize(Demo *demo);

static bool memory_type_from_properties(Demo *demo, uint32_t typeBits,
                                        vk::MemoryPropertyFlags requirements_mask,
                                        vk::MemoryAllocateInfo& mem_alloc) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((demo->memoryProperties.memoryTypes()[i].propertyFlags() &
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
        vk::ClearValue().depthStencil(vk::ClearDepthStencilValue{1.0, 0})
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
    demo->draw_cmd.bindIndexBuffer(demo->indices.buf,
                                   vk::DeviceSize{},
                                   vk::IndexType::eUint16);

    // demo->draw_cmd.draw(6, 1, 0, 0);
    demo->draw_cmd.drawIndexed(6, 1, 0, 0, 0);
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
    vkErr = demo->app.entryPoints.fpAcquireNextImageKHR(
      demo->device, demo->swapchain, UINT64_MAX, presentCompleteSemaphore,
      (vk::Fence)nullptr, &demo->current_buffer);
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
    vkErr = demo->app.entryPoints.fpQueuePresentKHR(demo->queue, &vkPresent);
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
    vkErr = demo->app.entryPoints.fpGetPhysicalDeviceSurfaceCapabilitiesKHR(
        demo->gpu, demo->surface, &vkSurfCapabilities);
    assert(vkErr == VK_SUCCESS);

    uint32_t presentModeCount;
    vkErr = demo->app.entryPoints.fpGetPhysicalDeviceSurfacePresentModesKHR(
        demo->gpu, demo->surface, &presentModeCount, nullptr);
    assert(vkErr == VK_SUCCESS);

    VkPresentModeKHR *presentModes = new VkPresentModeKHR[presentModeCount];
    assert(presentModes);

    vkErr = demo->app.entryPoints.fpGetPhysicalDeviceSurfacePresentModesKHR(
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
        .imageColorSpace(demo->colorSpace)
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
    vkErr = demo->app.entryPoints.fpCreateSwapchainKHR(
        demo->device, &vkSwapchainInfo, nullptr, &vkSwapchain);
    demo->swapchain = vkSwapchain;
    assert(vkErr == VK_SUCCESS);

    // If we just re-created an existing swapchain, we should destroy the old
    // swapchain at this point.
    // Note: destroying the swapchain also cleans up all its associated
    // presentable images once the platform is done with them.
    if (oldSwapchain != VK_NULL_HANDLE) {
        demo->app.entryPoints.fpDestroySwapchainKHR(demo->device, oldSwapchain, nullptr);
    }

    vkErr = demo->app.entryPoints.fpGetSwapchainImagesKHR(
        demo->device, demo->swapchain, &demo->swapchainImageCount, nullptr);
    assert(vkErr == VK_SUCCESS);

    VkImage *swapchainImages = new VkImage[demo->swapchainImageCount];
    vkErr = demo->app.entryPoints.fpGetSwapchainImagesKHR(
        demo->device, demo->swapchain, &demo->swapchainImageCount, swapchainImages);
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
                           TextureObject *tex_obj, vk::ImageTiling tiling,
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
                                       TextureObject *tex_obj) {
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
            TextureObject staging_texture;

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

static void demo_prepare_indices(Demo *demo) {
  const GLushort indices[] = {
    0, 1, 2, 2, 1, 3
  };
  const vk::BufferCreateInfo buf_info = vk::BufferCreateInfo()
      .size(sizeof(indices))
      .usage(vk::BufferUsageFlagBits::eIndexBuffer);

  vk::MemoryAllocateInfo mem_alloc;
  vk::MemoryRequirements mem_reqs;
  vk::Result U_ASSERT_ONLY err;
  bool U_ASSERT_ONLY pass;
  void *data;

  err = demo->device.createBuffer(&buf_info, nullptr, &demo->indices.buf);
  assert(err == vk::Result::eSuccess);

  demo->device.getBufferMemoryRequirements(demo->indices.buf, &mem_reqs);
  assert(err == vk::Result::eSuccess);

  mem_alloc.allocationSize(mem_reqs.size());
  pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits(),
                                     vk::MemoryPropertyFlagBits::eHostVisible,
                                     mem_alloc);
  assert(pass);

  err = demo->device.allocateMemory(&mem_alloc, nullptr, &demo->indices.mem);
  assert(err == vk::Result::eSuccess);

  err = demo->device.mapMemory(demo->indices.mem, 0, mem_alloc.allocationSize(),
                               vk::MemoryMapFlags{}, &data);
  assert(err == vk::Result::eSuccess);

  std::memcpy(data, indices, sizeof(indices));

  demo->device.unmapMemory(demo->indices.mem);

  err = demo->device.bindBufferMemory(demo->indices.buf, demo->indices.mem, 0);
  assert(err == vk::Result::eSuccess);
}

static void demo_prepare_vertices(Demo *demo) {
    const float vb[][5] = {
        /*      position             texcoord */
        { -1.0f, 0.0f, -1.0f,      0.0f, 0.0f },
        {  1.0f, 0.0f, -1.0f,      1.0f, 0.0f },
        { -1.0f, 0.0f,  1.0f,      0.5f, 1.0f },
        {  1.0f, 0.0f,  1.0f,      0.5f, 1.0f },
    };
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

    std::memcpy(data, vb, sizeof(vb));

    demo->device.unmapMemory(demo->vertices.mem);

    err = demo->device.bindBufferMemory(demo->vertices.buf, demo->vertices.mem, 0);
    assert(err == vk::Result::eSuccess);

    demo_prepare_indices(demo);

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
    const vk::DescriptorSetLayoutBinding layout_binding[2] = {
      vk::DescriptorSetLayoutBinding()
        .binding(0)
        .descriptorType(vk::DescriptorType::eCombinedImageSampler)
        .descriptorCount(DEMO_TEXTURE_COUNT)
        .stageFlags(vk::ShaderStageFlagBits::eFragment),
      vk::DescriptorSetLayoutBinding()
        .binding(1)
        .descriptorType(vk::DescriptorType::eUniformBuffer)
        .descriptorCount(1)
        .stageFlags(vk::ShaderStageFlagBits::eVertex)
    };

    const vk::DescriptorSetLayoutCreateInfo descriptor_layout =
      vk::DescriptorSetLayoutCreateInfo()
        .bindingCount(2)
        .pBindings(layout_binding);

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
    const vk::DescriptorPoolSize type_count[] = {
      vk::DescriptorPoolSize()
        .type(vk::DescriptorType::eCombinedImageSampler)
        .descriptorCount(DEMO_TEXTURE_COUNT),
      vk::DescriptorPoolSize()
        .type(vk::DescriptorType::eUniformBuffer)
        .descriptorCount(1)
    };

    const vk::DescriptorPoolCreateInfo descriptor_pool =
      vk::DescriptorPoolCreateInfo()
        .maxSets(1)
        .poolSizeCount(2)
        .pPoolSizes(type_count);

    vk::Result U_ASSERT_ONLY err;
    err = demo->device.createDescriptorPool(&descriptor_pool, nullptr,
                                            &demo->desc_pool);
    assert(err == vk::Result::eSuccess);
}

static void init_uniform_buffer(Demo *demo) {
    bool U_ASSERT_ONLY pass;

    /* VULKAN_KEY_START */
    vk::BufferCreateInfo buf_info = vk::BufferCreateInfo{}
        .usage(vk::BufferUsageFlagBits::eUniformBuffer)
        .size(16*sizeof(float))
        .sharingMode(vk::SharingMode::eExclusive);
    vk::chk(demo->device.createBuffer(&buf_info, NULL, &demo->uniformData.buf));

    vk::MemoryRequirements mem_reqs;
    demo->device.getBufferMemoryRequirements(demo->uniformData.buf, &mem_reqs);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize(mem_reqs.size());
    pass = memory_type_from_properties(demo, mem_reqs.memoryTypeBits(),
                                       vk::MemoryPropertyFlagBits::eHostVisible,
                                       alloc_info);
    assert(pass);

    vk::chk(demo->device.allocateMemory(&alloc_info, NULL,
                                        &demo->uniformData.mem));

    vk::chk(demo->device.bindBufferMemory(demo->uniformData.buf,
                                          demo->uniformData.mem, 0));

    demo->uniformData.bufferInfo.buffer(demo->uniformData.buf);
    demo->uniformData.bufferInfo.offset(0);
    demo->uniformData.bufferInfo.range(16*sizeof(float));
}

static void demo_prepare_descriptor_set(Demo *demo) {
    vk::DescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
    vk::WriteDescriptorSet writes[2];
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

    writes[0].dstBinding(0);
    writes[0].dstSet(demo->desc_set);
    writes[0].descriptorCount(DEMO_TEXTURE_COUNT);
    writes[0].descriptorType(vk::DescriptorType::eCombinedImageSampler);
    writes[0].pImageInfo(tex_descs);

    writes[1].dstBinding(1);
    writes[1].dstSet(demo->desc_set);
    writes[1].descriptorCount(1);
    writes[1].descriptorType(vk::DescriptorType::eUniformBuffer);
    writes[1].pBufferInfo(&demo->uniformData.bufferInfo);

    demo->device.updateDescriptorSets(2, writes, 0, nullptr);
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
        .queueFamilyIndex(demo->graphicsQueueNodeIndex)
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
    init_uniform_buffer(demo);

    demo_prepare_descriptor_pool(demo);
    demo_prepare_descriptor_set(demo);

    demo_prepare_framebuffers(demo);
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

    demo->device.destroyBuffer(demo->uniformData.buf, nullptr);
    demo->device.freeMemory(demo->uniformData.mem, nullptr);

    demo->device.destroyBuffer(demo->vertices.buf, nullptr);
    demo->device.freeMemory(demo->vertices.mem, nullptr);
    demo->device.destroyBuffer(demo->indices.buf, nullptr);
    demo->device.freeMemory(demo->indices.mem, nullptr);

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

    demo->app.entryPoints.fpDestroySwapchainKHR(demo->device, demo->swapchain, nullptr);
    delete[] demo->buffers;

    demo->device.destroy(nullptr);
    demo->inst.destroySurfaceKHR(demo->surface, nullptr);
    demo->debugCallback = nullptr;
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

    demo->device.destroyBuffer(demo->uniformData.buf, nullptr);
    demo->device.freeMemory(demo->uniformData.mem, nullptr);

    demo->device.destroyBuffer(demo->vertices.buf, nullptr);
    demo->device.freeMemory(demo->vertices.mem, nullptr);
    demo->device.destroyBuffer(demo->indices.buf, nullptr);
    demo->device.freeMemory(demo->indices.mem, nullptr);

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

DemoScene::DemoScene(GLFWwindow *window) : Scene(window) {
  glfwGetWindowSize(window_, &demo_.width, &demo_.height);
  demo_.window = window;

  demo_.inst = Initialize::CreateInstance(demo_.app);
  demo_.debugCallback = std::unique_ptr<Initialize::DebugCallback>{
      new Initialize::DebugCallback(demo_.inst)};
  demo_.gpu  = Initialize::CreatePhysicalDevice(demo_.inst, demo_.app);

  demo_.surface = Initialize::CreateSurface(demo_.inst, demo_.window);
  demo_.graphicsQueueNodeIndex = Initialize::SelectQraphicsQueueNodeIndex(
      demo_.gpu, demo_.surface, demo_.app);
  demo_.device = Initialize::CreateDevice(demo_.gpu, demo_.graphicsQueueNodeIndex,
                                          demo_.app);
  demo_.queue = Initialize::GetQueue(demo_.device, demo_.graphicsQueueNodeIndex);
  Initialize::GetSurfaceProperties(demo_.gpu, demo_.surface, demo_.app,
                                   demo_.format, demo_.colorSpace, demo_.memoryProperties);

  demo_prepare(&demo_);

  set_camera(addComponent<engine::FreeFlyCamera>(glm::radians(60.0), 0.1, 100, glm::dvec3(1, 1, -1)));
}

DemoScene::~DemoScene() {
  demo_cleanup(&demo_);
}

void DemoScene::render() {
  demo_draw(&demo_);
}

void DemoScene::update() {
  glm::mat4 mvp = scene()->camera()->projectionMatrix() *
                  scene()->camera()->cameraMatrix();

  uint8_t *pData;
  vk::chk(demo_.device.mapMemory(demo_.uniformData.mem, 0, sizeof(mvp),
                                 vk::MemoryMapFlags(), (void **)&pData));

  std::memcpy(pData, &mvp, sizeof(mvp));

  demo_.device.unmapMemory(demo_.uniformData.mem);

  // Wait for work to finish before updating MVP.
  demo_.device.waitIdle();
}

void DemoScene::screenResized(size_t width, size_t height) {
  demo_.width = width;
  demo_.height = width;
  demo_resize(&demo_);
}

